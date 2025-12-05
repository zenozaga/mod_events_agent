/*
 * service_b_nats.c
 * SERVICE B: Bridge using official NATS C client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <nats/nats.h>

#define NATS_URL "nats://127.0.0.1:5800"
#define SUBJECT "freeswitch.api"

volatile int running = 1;

void signal_handler(int sig) {
    printf("\n⚠️  Received signal %d, shutting down...\n", sig);
    running = 0;
}

char* process_command(const char *command) {
    static char response[512];
    time_t now = time(NULL);
    snprintf(response, sizeof(response),
             "{\"status\":\"ok\",\"echo\":\"%s\",\"timestamp\":%ld,\"from\":\"service_b\"}",
             command, now);
    return response;
}

void on_message(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure) {
    static int messages_processed = 0;
    natsConnection *conn = (natsConnection *)closure;
    
    const char *subject = natsMsg_GetSubject(msg);
    const char *data = natsMsg_GetData(msg);
    int len = natsMsg_GetDataLength(msg);
    const char *reply_to = natsMsg_GetReply(msg);
    
    printf("📨 Message received:\n");
    printf("   Subject: %s\n", subject);
    printf("   Reply-To: %s\n", reply_to ? reply_to : "(none)");
    printf("   Payload: %.*s\n", len, data);
    fflush(stdout);
    
    messages_processed++;
    
    if (reply_to && reply_to[0]) {
        char *response = process_command(data);
        printf("⚙️  Processing command...\n");
        fflush(stdout);
        
        usleep(100000);
        
        natsStatus s = natsConnection_PublishString(conn, reply_to, response);
        if (s == NATS_OK) {
            printf("✅ Response sent (#%d)\n\n", messages_processed);
        } else {
            printf("❌ Failed to send response: %s\n", natsStatus_GetText(s));
        }
        fflush(stdout);
    } else {
        printf("⚠️  No reply-to, skipping response\n\n");
        fflush(stdout);
    }
    
    natsMsg_Destroy(msg);
}

int main(void) {
    natsConnection *conn = NULL;
    natsSubscription *sub = NULL;
    natsOptions *opts = NULL;
    natsStatus s;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║  SERVICE B - Command Responder (Server)║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    fflush(stdout);
    
    // Create options
    s = natsOptions_Create(&opts);
    if (s != NATS_OK) {
        fprintf(stderr, "❌ Failed to create options: %s\n", natsStatus_GetText(s));
        return 1;
    }
    
    natsOptions_SetURL(opts, NATS_URL);
    
    // Connect to NATS
    s = natsConnection_Connect(&conn, opts);
    if (s != NATS_OK) {
        fprintf(stderr, "❌ Failed to connect to NATS: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return 1;
    }
    
    printf("✓ Connected to NATS (%s)\n", NATS_URL);
    fflush(stdout);
    
    // Subscribe to subject
    s = natsConnection_Subscribe(&sub, conn, SUBJECT, on_message, (void*)conn);
    if (s != NATS_OK) {
        fprintf(stderr, "❌ Failed to subscribe: %s\n", natsStatus_GetText(s));
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return 1;
    }
    
    // Set no limits on pending messages
    s = natsSubscription_SetPendingLimits(sub, -1, -1);
    if (s != NATS_OK) {
        fprintf(stderr, "⚠️  Warning: Could not set pending limits: %s\n", natsStatus_GetText(s));
    }
    
    // Flush to ensure subscription is registered
    s = natsConnection_Flush(conn);
    if (s != NATS_OK) {
        fprintf(stderr, "⚠️  Warning: Flush failed: %s\n", natsStatus_GetText(s));
    }
    
    printf("✓ Subscribed to: %s\n", SUBJECT);
    printf("✓ Subscription active\n");
    printf("\n🎧 Listening for commands...\n");
    printf("   Press Ctrl+C to stop\n\n");
    fflush(stdout);
    
    // Keep running
    while (running) {
        usleep(100000);
    }
    
    // Cleanup
    printf("\n📊 Shutting down...\n");
    fflush(stdout);
    
    natsSubscription_Destroy(sub);
    natsConnection_Destroy(conn);
    natsOptions_Destroy(opts);
    
    printf("✓ Service B stopped gracefully\n");
    return 0;
}
