/**
 * @file nats_command_client.c
 * @brief NATS Command Client - Send commands to FreeSWITCH via NATS Microservices
 * 
 * Demonstrates request-reply pattern for controlling FreeSWITCH:
 * - fs.cmd.originate: Originate calls
 * - fs.cmd.hangup: Hangup channels
 * - fs.cmd.bridge: Bridge channels
 * - fs.cmd.status: Get system status
 * - fs.cmd.execute: Execute dialplan apps
 * 
 * Usage:
 *   ./nats_command_client <command> [args]
 * 
 * Examples:
 *   ./nats_command_client status
 *   ./nats_command_client originate user/1000 9999
 *   ./nats_command_client hangup <uuid>
 *   ./nats_command_client bridge <uuid> user/1001
 *   ./nats_command_client execute <uuid> playback /tmp/hello.wav
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <nats.h>
#include <cjson/cJSON.h>

static volatile sig_atomic_t keepRunning = 1;

void signalHandler(int sig) {
    keepRunning = 0;
}

/**
 * Send command and wait for response
 */
natsStatus send_command(natsConnection *conn, const char *subject, const char *json_data) {
    natsStatus s = NATS_OK;
    natsMsg *reply = NULL;
    
    printf("📤 Sending command to: %s\n", subject);
    printf("📋 Request: %s\n\n", json_data);
    
    // Send request and wait for reply (5 second timeout)
    s = natsConnection_RequestString(&reply, conn, subject, json_data, 5000);
    
    if (s != NATS_OK) {
        fprintf(stderr, "❌ Request failed: %s\n", natsStatus_GetText(s));
        return s;
    }
    
    // Parse response
    const char *response_data = natsMsg_GetData(reply);
    printf("📥 Response:\n");
    
    cJSON *json = cJSON_Parse(response_data);
    if (json) {
        char *formatted = cJSON_Print(json);
        printf("%s\n\n", formatted);
        
        // Check success status
        cJSON *success = cJSON_GetObjectItem(json, "success");
        if (success && success->valueint) {
            printf("✅ Command executed successfully\n");
        } else {
            printf("❌ Command failed\n");
            cJSON *message = cJSON_GetObjectItem(json, "message");
            if (message) {
                printf("   Error: %s\n", message->valuestring);
            }
        }
        
        free(formatted);
        cJSON_Delete(json);
    } else {
        printf("%s\n", response_data);
    }
    
    natsMsg_Destroy(reply);
    return NATS_OK;
}

/**
 * Command: status
 */
natsStatus cmd_status(natsConnection *conn) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", "status");
    
    char *json_str = cJSON_PrintUnformatted(json);
    natsStatus s = send_command(conn, "fs.cmd.status", json_str);
    
    free(json_str);
    cJSON_Delete(json);
    return s;
}

/**
 * Command: originate
 * Usage: originate <endpoint> <extension> [context] [caller_id]
 */
natsStatus cmd_originate(natsConnection *conn, int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s originate <endpoint> <extension> [context] [caller_id]\n", argv[0]);
        fprintf(stderr, "Example: %s originate user/1000 9999 default \"Test Call <1234>\"\n", argv[0]);
        return NATS_INVALID_ARG;
    }
    
    const char *endpoint = argv[2];
    const char *extension = argv[3];
    const char *context = (argc > 4) ? argv[4] : "default";
    const char *caller_id = (argc > 5) ? argv[5] : "";
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "endpoint", endpoint);
    cJSON_AddStringToObject(json, "extension", extension);
    cJSON_AddStringToObject(json, "context", context);
    if (strlen(caller_id) > 0) {
        cJSON_AddStringToObject(json, "caller_id", caller_id);
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    natsStatus s = send_command(conn, "fs.cmd.originate", json_str);
    
    free(json_str);
    cJSON_Delete(json);
    return s;
}

/**
 * Command: hangup
 * Usage: hangup <uuid> [cause]
 */
natsStatus cmd_hangup(natsConnection *conn, int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s hangup <uuid> [cause]\n", argv[0]);
        fprintf(stderr, "Example: %s hangup 12345678-1234-1234-1234-123456789012 NORMAL_CLEARING\n", argv[0]);
        return NATS_INVALID_ARG;
    }
    
    const char *uuid = argv[2];
    const char *cause = (argc > 3) ? argv[3] : "NORMAL_CLEARING";
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "uuid", uuid);
    cJSON_AddStringToObject(json, "cause", cause);
    
    char *json_str = cJSON_PrintUnformatted(json);
    natsStatus s = send_command(conn, "fs.cmd.hangup", json_str);
    
    free(json_str);
    cJSON_Delete(json);
    return s;
}

/**
 * Command: bridge
 * Usage: bridge <uuid> <endpoint>
 */
natsStatus cmd_bridge(natsConnection *conn, int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s bridge <uuid> <endpoint>\n", argv[0]);
        fprintf(stderr, "Example: %s bridge 12345678-1234-1234-1234-123456789012 user/1001\n", argv[0]);
        return NATS_INVALID_ARG;
    }
    
    const char *uuid = argv[2];
    const char *endpoint = argv[3];
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "uuid", uuid);
    cJSON_AddStringToObject(json, "endpoint", endpoint);
    
    char *json_str = cJSON_PrintUnformatted(json);
    natsStatus s = send_command(conn, "fs.cmd.bridge", json_str);
    
    free(json_str);
    cJSON_Delete(json);
    return s;
}

/**
 * Command: execute
 * Usage: execute <uuid> <app> [args]
 */
natsStatus cmd_execute(natsConnection *conn, int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s execute <uuid> <app> [args]\n", argv[0]);
        fprintf(stderr, "Example: %s execute 12345678-1234-1234-1234-123456789012 playback /tmp/hello.wav\n", argv[0]);
        return NATS_INVALID_ARG;
    }
    
    const char *uuid = argv[2];
    const char *app = argv[3];
    const char *args = (argc > 4) ? argv[4] : "";
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "uuid", uuid);
    cJSON_AddStringToObject(json, "app", app);
    if (strlen(args) > 0) {
        cJSON_AddStringToObject(json, "args", args);
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    natsStatus s = send_command(conn, "fs.cmd.execute", json_str);
    
    free(json_str);
    cJSON_Delete(json);
    return s;
}

/**
 * Print usage
 */
void print_usage(const char *prog) {
    printf("NATS Command Client for FreeSWITCH\n");
    printf("==================================\n\n");
    printf("Usage: %s <command> [args]\n\n", prog);
    printf("Commands:\n");
    printf("  status\n");
    printf("      Get FreeSWITCH system status and microservice statistics\n\n");
    printf("  originate <endpoint> <extension> [context] [caller_id]\n");
    printf("      Originate a new call\n");
    printf("      Example: %s originate user/1000 9999 default \"Test <1234>\"\n\n", prog);
    printf("  hangup <uuid> [cause]\n");
    printf("      Hangup a channel\n");
    printf("      Example: %s hangup 12345678-1234-1234-1234-123456789012 NORMAL_CLEARING\n\n", prog);
    printf("  bridge <uuid> <endpoint>\n");
    printf("      Bridge a channel to another endpoint\n");
    printf("      Example: %s bridge 12345678-1234-1234-1234-123456789012 user/1001\n\n", prog);
    printf("  execute <uuid> <app> [args]\n");
    printf("      Execute a dialplan application on a channel\n");
    printf("      Example: %s execute 12345678-1234-1234-1234-123456789012 playback /tmp/hello.wav\n\n", prog);
    printf("Environment:\n");
    printf("  NATS_URL: NATS server URL (default: nats://127.0.0.1:4222)\n\n");
}

int main(int argc, char *argv[]) {
    natsConnection *conn = NULL;
    natsOptions *opts = NULL;
    natsStatus s = NATS_OK;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *command = argv[1];
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Get NATS URL from environment or use default
    const char *nats_url = getenv("NATS_URL");
    if (nats_url == NULL) {
        nats_url = "nats://127.0.0.1:4222";
    }
    
    printf("🔌 Connecting to NATS: %s\n", nats_url);
    
    // Create options
    s = natsOptions_Create(&opts);
    if (s != NATS_OK) {
        fprintf(stderr, "Failed to create options: %s\n", natsStatus_GetText(s));
        return 1;
    }
    
    natsOptions_SetURL(opts, nats_url);
    natsOptions_SetTimeout(opts, 5000); // 5 second timeout
    
    // Connect
    s = natsConnection_Connect(&conn, opts);
    if (s != NATS_OK) {
        fprintf(stderr, "Failed to connect: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return 1;
    }
    
    printf("✅ Connected to NATS\n\n");
    
    // Execute command
    if (strcmp(command, "status") == 0) {
        s = cmd_status(conn);
    } else if (strcmp(command, "originate") == 0) {
        s = cmd_originate(conn, argc, argv);
    } else if (strcmp(command, "hangup") == 0) {
        s = cmd_hangup(conn, argc, argv);
    } else if (strcmp(command, "bridge") == 0) {
        s = cmd_bridge(conn, argc, argv);
    } else if (strcmp(command, "execute") == 0) {
        s = cmd_execute(conn, argc, argv);
    } else if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0) {
        print_usage(argv[0]);
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", command);
        print_usage(argv[0]);
        s = NATS_INVALID_ARG;
    }
    
    // Cleanup
    natsConnection_Destroy(conn);
    natsOptions_Destroy(opts);
    
    printf("\n👋 Disconnected\n");
    
    return (s == NATS_OK) ? 0 : 1;
}
