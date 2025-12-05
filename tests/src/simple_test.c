/*
 * simple_test.c
 * Simple NATS test: Two modes
 * 1. SERVER: subscribe and wait for messages
 * 2. CLIENT: publish message to subject
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <nats/nats.h>

static int received = 0;

void onMsg(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    const char *reply = natsMsg_GetReply(msg);
    
    printf("✅ Received message on [%s]: %.*s\n",
           natsMsg_GetSubject(msg),
           natsMsg_GetDataLength(msg),
           natsMsg_GetData(msg));
    
    if (reply) {
        printf("   Reply-To: %s\n", reply);
        // Send response
        natsConnection_PublishString(nc, reply, "{\"status\":\"ok\",\"response\":\"acknowledged\"}");
        printf("   ✓ Sent response to: %s\n", reply);
    }
    
    received++;
    natsMsg_Destroy(msg);
}

void server_mode(const char *subject)
{
    natsConnection *conn = NULL;
    natsSubscription *sub = NULL;
    natsStatus s;
    
    printf("=== SERVER MODE ===\n\n");
    
    s = natsConnection_ConnectTo(&conn, "nats://127.0.0.1:5800");
    if (s != NATS_OK) {
        printf("❌ Connect failed: %s\n", natsStatus_GetText(s));
        return;
    }
    printf("✓ Connected to NATS\n");
    
    s = natsConnection_Subscribe(&sub, conn, subject, onMsg, (void*)conn);
    if (s != NATS_OK) {
        printf("❌ Subscribe failed: %s\n", natsStatus_GetText(s));
        natsConnection_Destroy(conn);
        return;
    }
    
    s = natsConnection_Flush(conn);
    printf("✓ Subscribed to: %s\n\n", subject);
    printf("🎧 Listening for messages (Ctrl+C to stop)...\n\n");
    
    while (1) {
        nats_Sleep(100);
    }
    
    natsSubscription_Destroy(sub);
    natsConnection_Destroy(conn);
}

void client_mode(const char *subject, const char *message, int with_reply)
{
    natsConnection *conn = NULL;
    natsSubscription *sub = NULL;
    natsMsg *reply_msg = NULL;
    natsStatus s;
    char inbox[256];
    
    printf("=== CLIENT MODE ===\n\n");
    
    s = natsConnection_ConnectTo(&conn, "nats://127.0.0.1:5800");
    if (s != NATS_OK) {
        printf("❌ Connect failed: %s\n", natsStatus_GetText(s));
        return;
    }
    printf("✓ Connected to NATS\n");
    
    if (with_reply) {
        // Create inbox for response
        snprintf(inbox, sizeof(inbox), "_INBOX.%ld", (long)time(NULL));
        
        s = natsConnection_SubscribeSync(&sub, conn, inbox);
        if (s != NATS_OK) {
            printf("❌ Failed to create inbox: %s\n", natsStatus_GetText(s));
            natsConnection_Destroy(conn);
            return;
        }
        
        s = natsSubscription_AutoUnsubscribe(sub, 1);
        printf("✓ Created inbox: %s\n", inbox);
        
        // Publish with reply-to
        s = natsConnection_PublishRequest(conn, subject, inbox, message, strlen(message));
        if (s != NATS_OK) {
            printf("❌ Publish failed: %s\n", natsStatus_GetText(s));
        } else {
            printf("✓ Published to: %s (with reply-to: %s)\n", subject, inbox);
            printf("  Message: %s\n\n", message);
        }
        
        printf("⏳ Waiting for response (10s timeout)...\n");
        s = natsSubscription_NextMsg(&reply_msg, sub, 10000);
        if (s == NATS_OK) {
            printf("✅ Response received: %.*s\n",
                   natsMsg_GetDataLength(reply_msg),
                   natsMsg_GetData(reply_msg));
            natsMsg_Destroy(reply_msg);
        } else {
            printf("❌ No response: %s\n", natsStatus_GetText(s));
        }
        
        natsSubscription_Destroy(sub);
    } else {
        // Simple publish
        s = natsConnection_PublishString(conn, subject, message);
        if (s != NATS_OK) {
            printf("❌ Publish failed: %s\n", natsStatus_GetText(s));
        } else {
            printf("✓ Published to: %s\n", subject);
            printf("  Message: %s\n", message);
        }
    }
    
    natsConnection_Destroy(conn);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s server <subject>              - Run in server mode\n", argv[0]);
        printf("  %s pub <subject> <message>       - Publish message\n", argv[0]);
        printf("  %s req <subject> <message>       - Request-reply pattern\n", argv[0]);
        printf("\nExamples:\n");
        printf("  %s server freeswitch.api\n", argv[0]);
        printf("  %s pub test.subject \"Hello\"\n", argv[0]);
        printf("  %s req freeswitch.api \"status\"\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "server") == 0) {
        if (argc < 3) {
            printf("Error: Missing subject\n");
            return 1;
        }
        server_mode(argv[2]);
    }
    else if (strcmp(argv[1], "pub") == 0) {
        if (argc < 4) {
            printf("Error: Missing subject or message\n");
            return 1;
        }
        client_mode(argv[2], argv[3], 0);
    }
    else if (strcmp(argv[1], "req") == 0) {
        if (argc < 4) {
            printf("Error: Missing subject or message\n");
            return 1;
        }
        client_mode(argv[2], argv[3], 1);
    }
    else {
        printf("Error: Unknown mode '%s'\n", argv[1]);
        return 1;
    }
    
    return 0;
}
