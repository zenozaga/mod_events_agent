const http = require('http');
const fs = require('fs');
const path = require('path');
const { connect } = require('nats');

const PORT = process.env.PORT || 3000;
const NATS_URL = process.env.NATS_URL || 'nats://localhost:5800';
const NODE_ID = process.env.NODE_ID || 'agent_node_1';

let nc = null;
let eventClients = [];


async function connectNATS() {
    try {
        nc = await connect({ servers: NATS_URL });
        console.log('✅ Connected to NATS at', NATS_URL);

        subscribeToEvents();
    } catch (error) {
        console.error('❌ Failed to connect to NATS:', error);
        setTimeout(connectNATS, 3000);
    }
}

function subscribeToEvents() {
    const subjects = [
        'freeswitch.events.channel.park',
        'freeswitch.events.channel.hangup',
        'freeswitch.events.channel.answer'
    ];

    subjects.forEach(async (subject) => {
        const sub = nc.subscribe(subject);
        for await (const msg of sub) {
            try {
                const data = JSON.parse(new TextDecoder().decode(msg.data));
                broadcastEvent({ type: subject, data });
            } catch (error) {
                console.error('Error processing message:', error);
            }
        }
    });
}

async function sendCommand(command, args) {
    if (!nc) {
        return { success: false, message: 'Not connected to NATS' };
    }

    const subject = `freeswitch.api.${NODE_ID}`;
    const start_time = Date.now();
    const request = JSON.stringify({ command, args });

    try {
        const response = await nc.request(subject, new TextEncoder().encode(request), { timeout: 5000 });
       
        const end_time = Date.now();
       
        return Object.assign(JSON.parse(new TextDecoder().decode(response.data)), {
            start_time,
        });
    } catch (error) {
        return { success: false, message: String(error) };
    }
}

function broadcastEvent(event) {
    const data = `data: ${JSON.stringify(event)}\n\n`;
    eventClients.forEach((client, index) => {
        try {
            client.write(data);
        } catch (error) {
            eventClients.splice(index, 1);
        }
    });
}

function serveStatic(res, filePath) {
    const extname = path.extname(filePath);
    const contentTypes = {
        '.html': 'text/html',
        '.js': 'text/javascript',
        '.css': 'text/css',
        '.json': 'application/json'
    };
    const contentType = contentTypes[extname] || 'text/plain';

    fs.readFile(filePath, (error, content) => {
        if (error) {
            res.writeHead(404);
            res.end('404 Not Found');
        } else {
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(content, 'utf-8');
        }
    });
}

function parseBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => body += chunk.toString());
        req.on('end', () => {
            try {
                resolve(body ? JSON.parse(body) : {});
            } catch (error) {
                reject(error);
            }
        });
    });
}

const server = http.createServer(async (req, res) => {
    const { method, url } = req;

    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    if (url === '/' || url === '/index.html') {
        serveStatic(res, path.join(__dirname, 'public', 'index.html'));
    } else if (url === '/app.js') {
        serveStatic(res, path.join(__dirname, 'public', 'app.js'));
    } else if (url === '/api/events') {
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        eventClients.push(res);
        res.write('data: {"type":"connected"}\n\n');

        req.on('close', () => {
            eventClients = eventClients.filter(client => client !== res);
        });
    } else if (url === '/api/command' && method === 'POST') {
        try {
            const body = await parseBody(req);
            const result = await sendCommand(body.command, body.args || '');
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(result));
        } catch (error) {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ success: false, message: String(error) }));
        }
    } else if (url === '/api/calls' && method === 'GET') {
        const result = await sendCommand('show', 'calls');
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(result));
    } else if (url === '/api/calls/answer' && method === 'POST') {
        try {
            const body = await parseBody(req);
            const destination = body.destination || 'user/1001';
            const result = await sendCommand('uuid_bridge', `${body.uuid} ${destination}`);
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(result));
        } catch (error) {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ success: false, message: String(error) }));
        }
    } else if (url === '/api/calls/hangup' && method === 'POST') {
        try {
            const body = await parseBody(req);
            const cause = body.cause || 'NORMAL_CLEARING';
            const result = await sendCommand('uuid_kill', `${body.uuid} ${cause}`);
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(result));
        } catch (error) {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ success: false, message: String(error) }));
        }
    } else if (url === '/api/calls/transfer' && method === 'POST') {
        try {
            const body = await parseBody(req);
            const context = body.context || 'default';
            const result = await sendCommand('uuid_transfer', `${body.uuid} ${body.extension} XML ${context}`);
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(result));
        } catch (error) {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ success: false, message: String(error) }));
        }
    } else if (url === '/api/calls/originate' && method === 'POST') {
        try {
            const body = await parseBody(req);
            const context = body.context || 'default';
            const result = await sendCommand('originate', `${body.endpoint} ${body.destination} XML ${context}`);
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(result));
        } catch (error) {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ success: false, message: String(error) }));
        }
    } else {
        res.writeHead(404);
        res.end('404 Not Found');
    }
});

server.listen(PORT, () => {
    console.log('='.repeat(60));
    console.log('FreeSWITCH Call Control API');
    console.log('='.repeat(60));
    console.log(`Server:      http://localhost:${PORT}`);
    console.log(`NATS:        ${NATS_URL}`);
    console.log(`Node ID:     ${NODE_ID}`);
    console.log('='.repeat(60));
    
    connectNATS();
});
