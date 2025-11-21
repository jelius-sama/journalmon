#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define VERSION "1.0.0"
#define MAX_LINE 8192
#define MAX_CMD 16384
#define CONFIG_PATH_USER ".config/journalmon/config"
#define CONFIG_PATH_SYSTEM "/etc/journalmon/config"

#define YELLOW "\x1b[33m"
#define RED    "\x1b[31m"
#define GREEN  "\x1b[32m"
#define CYAN   "\x1b[36m"
#define RESET  "\x1b[0m"

#define WARN(text)  YELLOW "[WARN] "  text RESET
#define ERROR(text) RED    "[ERROR] " text RESET
#define OK(text)    GREEN  "[OK] "    text RESET
#define INFO(text)  CYAN   "[INFO] "  text RESET

typedef struct {
    char recipient[256];
    char mailer_path[512];
    int min_priority;  // 0-7, where 0=emerg, 3=err, 4=warning
    int batch_window;  // seconds to batch errors before sending
    char filters[1024]; // comma-separated service filters
} Config;

static volatile int running = 1;
static Config config;

void signal_handler(int sig) {
    running = 0;
    printf(ERROR("\nCaught signal %d, shutting down gracefully...\n"), sig);
}

char* html_escape(const char* str) {
    if (!str) return strdup("");
    
    size_t len = strlen(str);
    size_t new_len = len * 6 + 1; // Worst case: all chars need escaping
    char* escaped = malloc(new_len);
    if (!escaped) return strdup("");
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '&':
                strcpy(&escaped[j], "&amp;");
                j += 5;
                break;
            case '<':
                strcpy(&escaped[j], "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(&escaped[j], "&gt;");
                j += 4;
                break;
            case '"':
                strcpy(&escaped[j], "&quot;");
                j += 6;
                break;
            case '\'':
                strcpy(&escaped[j], "&#39;");
                j += 5;
                break;
            default:
                escaped[j++] = str[i];
        }
    }
    escaped[j] = '\0';
    return escaped;
}

char* get_priority_badge(int priority) {
    switch (priority) {
        case 0: return "EMERGENCY";
        case 1: return "ALERT";
        case 2: return "CRITICAL";
        case 3: return "ERROR";
        case 4: return "WARNING";
        case 5: return "NOTICE";
        case 6: return "INFO";
        case 7: return "DEBUG";
        default: return "UNKNOWN";
    }
}

char* get_priority_color(int priority) {
    switch (priority) {
        case 0:
        case 1:
        case 2:
        case 3: return "#ef4444"; // Red
        case 4: return "#f59e0b"; // Amber
        case 5: return "#3b82f6"; // Blue
        case 6: return "#10b981"; // Green
        case 7: return "#6b7280"; // Gray
        default: return "#000000";
    }
}

char* create_html_email(const char* hostname, const char* service, const char* message, 
                        const char* timestamp, int priority, const char* unit) {
    char* html = malloc(MAX_CMD);
    if (!html) return NULL;
    
    char* escaped_message = html_escape(message);
    char* escaped_service = html_escape(service);
    char* escaped_unit = html_escape(unit);
    char* escaped_hostname = html_escape(hostname);
    
    const char* badge = get_priority_badge(priority);
    const char* color = get_priority_color(priority);
    
    snprintf(html, MAX_CMD,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <meta name=\"color-scheme\" content=\"dark light\">\n"
        "    <title>System Error Alert</title>\n"
        "</head>\n"
        "<body style=\"margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; background: linear-gradient(135deg, #1a1a2e 0%%, #16213e 100%%); color: #e0e0e0;\">\n"
        "    <div style=\"max-width: 700px; margin: 40px auto; background: linear-gradient(145deg, #0f1419 0%%, #1a1f2e 100%%); border-radius: 20px; box-shadow: 0 20px 60px rgba(0,0,0,0.5), 0 0 0 1px rgba(255,255,255,0.05); overflow: hidden;\">\n"
        "        \n"
        "        <!-- Header -->\n"
        "        <div style=\"background: linear-gradient(135deg, %s 0%%, %s88 100%%); padding: 40px 30px; text-align: center; position: relative; overflow: hidden;\">\n"
        "            <div style=\"position: absolute; top: 0; left: 0; right: 0; bottom: 0; background: url('data:image/svg+xml,%%3Csvg width=\"100\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\"%%3E%%3Cpattern id=\"grid\" width=\"20\" height=\"20\" patternUnits=\"userSpaceOnUse\"%%3E%%3Cpath d=\"M 20 0 L 0 0 0 20\" fill=\"none\" stroke=\"rgba(255,255,255,0.03)\" stroke-width=\"1\"/%%3E%%3C/pattern%%3E%%3Crect width=\"100\" height=\"100\" fill=\"url(%%23grid)\"/%%3E%%3C/svg%%3E'); opacity: 0.3;\"></div>\n"
        "            <div style=\"position: relative; z-index: 1;\">\n"
        "                <div style=\"font-size: 56px; margin-bottom: 10px; filter: drop-shadow(0 4px 8px rgba(0,0,0,0.3));\">⚠️</div>\n"
        "                <h1 style=\"margin: 0; font-size: 32px; font-weight: 700; color: white; text-shadow: 0 2px 10px rgba(0,0,0,0.5); letter-spacing: -0.5px;\">System Alert</h1>\n"
        "                <p style=\"margin: 10px 0 0 0; color: rgba(255,255,255,0.9); font-size: 15px; font-weight: 500;\">journalctl monitor detected an issue</p>\n"
        "            </div>\n"
        "        </div>\n"
        "        \n"
        "        <!-- Content -->\n"
        "        <div style=\"padding: 40px 30px;\">\n"
        "            \n"
        "            <!-- Priority Badge -->\n"
        "            <div style=\"margin-bottom: 30px;\">\n"
        "                <span style=\"display: inline-block; background: linear-gradient(135deg, %s 0%%, %s 100%%); color: white; padding: 10px 20px; border-radius: 50px; font-size: 14px; font-weight: 600; letter-spacing: 0.5px; box-shadow: 0 4px 15px rgba(0,0,0,0.3), inset 0 1px 0 rgba(255,255,255,0.2);\">\n"
        "                    %s\n"
        "                </span>\n"
        "            </div>\n"
        "            \n"
        "            <!-- Info Grid -->\n"
        "            <div style=\"background: rgba(255,255,255,0.03); border-radius: 16px; padding: 25px; margin-bottom: 30px; border: 1px solid rgba(255,255,255,0.06); backdrop-filter: blur(10px);\">\n"
        "                <table style=\"width: 100%%; border-collapse: collapse;\">\n"
        "                    <tr>\n"
        "                        <td style=\"padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.05);\">\n"
        "                            <span style=\"color: #8b92a7; font-size: 13px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px;\">🖥️ Host</span>\n"
        "                        </td>\n"
        "                        <td style=\"padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.05); text-align: right;\">\n"
        "                            <span style=\"color: #e0e0e0; font-size: 15px; font-weight: 500;\">%s</span>\n"
        "                        </td>\n"
        "                    </tr>\n"
        "                    <tr>\n"
        "                        <td style=\"padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.05);\">\n"
        "                            <span style=\"color: #8b92a7; font-size: 13px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px;\">⚙️ Service</span>\n"
        "                        </td>\n"
        "                        <td style=\"padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.05); text-align: right;\">\n"
        "                            <span style=\"color: #e0e0e0; font-size: 15px; font-weight: 500; font-family: 'Courier New', monospace;\">%s</span>\n"
        "                        </td>\n"
        "                    </tr>\n"
        "                    <tr>\n"
        "                        <td style=\"padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.05);\">\n"
        "                            <span style=\"color: #8b92a7; font-size: 13px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px;\">📦 Unit</span>\n"
        "                        </td>\n"
        "                        <td style=\"padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.05); text-align: right;\">\n"
        "                            <span style=\"color: #e0e0e0; font-size: 15px; font-weight: 500; font-family: 'Courier New', monospace;\">%s</span>\n"
        "                        </td>\n"
        "                    </tr>\n"
        "                    <tr>\n"
        "                        <td style=\"padding: 12px 0;\">\n"
        "                            <span style=\"color: #8b92a7; font-size: 13px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px;\">🕐 Time</span>\n"
        "                        </td>\n"
        "                        <td style=\"padding: 12px 0; text-align: right;\">\n"
        "                            <span style=\"color: #e0e0e0; font-size: 15px; font-weight: 500;\">%s</span>\n"
        "                        </td>\n"
        "                    </tr>\n"
        "                </table>\n"
        "            </div>\n"
        "            \n"
        "            <!-- Error Message -->\n"
        "            <div style=\"background: linear-gradient(145deg, rgba(0,0,0,0.3) 0%%, rgba(0,0,0,0.2) 100%%); border-left: 4px solid %s; border-radius: 12px; padding: 20px 24px; margin-bottom: 30px; box-shadow: inset 0 2px 8px rgba(0,0,0,0.3);\">\n"
        "                <div style=\"color: #8b92a7; font-size: 12px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 12px;\">📄 Error Message</div>\n"
        "                <pre style=\"margin: 0; color: #f0f0f0; font-size: 14px; line-height: 1.6; font-family: 'Courier New', Consolas, monospace; white-space: pre-wrap; word-wrap: break-word; overflow-wrap: break-word;\">%s</pre>\n"
        "            </div>\n"
        "            \n"
        "            <!-- Action Footer -->\n"
        "            <div style=\"background: linear-gradient(135deg, rgba(59, 130, 246, 0.1) 0%%, rgba(99, 102, 241, 0.1) 100%%); border: 1px solid rgba(59, 130, 246, 0.2); border-radius: 12px; padding: 20px; text-align: center;\">\n"
        "                <p style=\"margin: 0 0 15px 0; color: #a0aec0; font-size: 14px;\">💡 <strong>Quick Actions</strong></p>\n"
        "                <code style=\"background: rgba(0,0,0,0.4); color: #60a5fa; padding: 10px 16px; border-radius: 8px; font-size: 13px; display: inline-block; border: 1px solid rgba(96, 165, 250, 0.2);\">journalctl -u %s -n 50 --no-pager</code>\n"
        "            </div>\n"
        "        </div>\n"
        "        \n"
        "        <!-- Footer -->\n"
        "        <div style=\"background: rgba(0,0,0,0.3); padding: 25px 30px; text-align: center; border-top: 1px solid rgba(255,255,255,0.05);\">\n"
        "            <p style=\"margin: 0 0 8px 0; color: #6b7280; font-size: 13px;\">📧 Automated alert from <strong style=\"color: #8b92a7;\">journalmon v%s</strong></p>\n"
        "            <p style=\"margin: 0; color: #4b5563; font-size: 12px;\">Monitoring your system's health 24/7</p>\n"
        "        </div>\n"
        "        \n"
        "    </div>\n"
        "    \n"
        "    <!-- Ambient background effects -->\n"
        "    <div style=\"text-align: center; margin-top: 30px; padding-bottom: 40px;\">\n"
        "        <p style=\"color: #4b5563; font-size: 12px; margin: 0;\">This is an automated message. Please do not reply.</p>\n"
        "    </div>\n"
        "</body>\n"
        "</html>",
        color, color,  // Header gradient
        color, color,  // Badge gradient
        badge,         // Priority badge text
        escaped_hostname,
        escaped_service,
        escaped_unit,
        timestamp,
        color,         // Border color
        escaped_message,
        escaped_service,
        VERSION
    );
    
    free(escaped_message);
    free(escaped_service);
    free(escaped_unit);
    free(escaped_hostname);
    
    return html;
}

int send_email(const char* subject, const char* html_body) {
    // Create temporary file for HTML body
    char temp_file[] = "/tmp/journalmon_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) {
        fprintf(stderr, ERROR("Failed to create temp file: %s\n"), strerror(errno));
        return -1;
    }
    
    write(fd, html_body, strlen(html_body));
    close(fd);
    
    // Build mailer command
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd),
        "%s --c \"%s\" --to \"%s\" --subject \"%s\" --body \"$(cat %s)\" 2>&1",
        "/home/kazuma/.config/mailer/config.json",
        config.mailer_path,
        config.recipient,
        subject,
        temp_file
    );
    
    // Execute mailer
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, ERROR("Failed to execute mailer: %s\n"), strerror(errno));
        unlink(temp_file);
        return -1;
    }
    
    char output[1024];
    while (fgets(output, sizeof(output), pipe) != NULL) {
        printf("  📧 %s", output);
    }
    
    int status = pclose(pipe);
    unlink(temp_file);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf(OK("Email sent successfully\n"));
        return 0;
    } else {
        fprintf(stderr, ERROR("Mailer failed with status %d\n"), WEXITSTATUS(status));
        return -1;
    }
}

int load_config(const char* config_path) {
    FILE* f = fopen(config_path, "r");
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        
        char key[128], value[384];
        if (sscanf(line, "%127[^=]=%383[^\n]", key, value) == 2) {
            // Trim whitespace
            char* k = key;
            char* v = value;
            while (isspace(*k)) k++;
            while (isspace(*v)) v++;
            
            if (strcmp(k, "recipient") == 0) {
                strncpy(config.recipient, v, sizeof(config.recipient) - 1);
            } else if (strcmp(k, "mailer_path") == 0) {
                strncpy(config.mailer_path, v, sizeof(config.mailer_path) - 1);
            } else if (strcmp(k, "min_priority") == 0) {
                config.min_priority = atoi(v);
            } else if (strcmp(k, "batch_window") == 0) {
                config.batch_window = atoi(v);
            } else if (strcmp(k, "filters") == 0) {
                strncpy(config.filters, v, sizeof(config.filters) - 1);
            }
        }
    }
    
    fclose(f);
    return 0;
}

void print_banner() {
    printf("\n");
    printf("    ╔═══════════════════════════════════════════════════════╗\n");
    printf("    ║                                                       ║\n");
    printf("    ║           JOURNALMON - System Error Monitor           ║\n");
    printf("    ║                   Version %s                      ║\n", VERSION);
    printf("    ║                                                       ║\n");
    printf("    ╚═══════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void show_help() {
    print_banner();
    printf("📚 USAGE:\n");
    printf("  journalmon [OPTIONS]\n\n");
    printf("🔧 OPTIONS:\n");
    printf("  -c, --config PATH    Path to config file\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n\n");
    printf("⚙️  CONFIGURATION:\n");
    printf("  Config file locations (in order of precedence):\n");
    printf("    1. ~/.config/journalmon/config\n");
    printf("    2. /etc/journalmon/config\n\n");
    printf("  Config file format:\n");
    printf("    recipient=admin@example.com\n");
    printf("    mailer_path=/usr/local/bin/mailer\n");
    printf("    min_priority=3          # 0=emerg, 3=error, 4=warning, 7=debug\n");
    printf("    batch_window=60         # seconds to batch errors\n");
    printf("    filters=nginx,postgres  # optional: specific services to monitor\n\n");
    printf("📖 EXAMPLES:\n");
    printf("  # Run with default config:\n");
    printf("  journalmon\n\n");
    printf("  # Run with custom config:\n");
    printf("  journalmon -c /path/to/config\n\n");
    printf("  # Install as systemd service:\n");
    printf("  sudo systemctl enable journalmon\n");
    printf("  sudo systemctl start journalmon\n\n");
}

int main(int argc, char* argv[]) {
    char* config_path = NULL;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("journalmon version %s\n", VERSION);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                fprintf(stderr, ERROR("Error: -c requires a path argument\n"));
                return 1;
            }
        }
    }
    
    // Load configuration
    config.min_priority = 3;  // Default: ERROR and above
    config.batch_window = 60;
    strcpy(config.mailer_path, "mailer");
    
    int config_loaded = 0;
    if (config_path) {
        config_loaded = (load_config(config_path) == 0);
    } else {
        // Try user config
        char user_config[512];
        const char* home = getenv("HOME");
        if (home) {
            snprintf(user_config, sizeof(user_config), "%s/%s", home, CONFIG_PATH_USER);
            config_loaded = (load_config(user_config) == 0);
        }
        
        // Try system config
        if (!config_loaded) {
            config_loaded = (load_config(CONFIG_PATH_SYSTEM) == 0);
        }
    }
    
    if (!config_loaded || strlen(config.recipient) == 0) {
        fprintf(stderr, ERROR("Error: No valid configuration found.\n"));
        fprintf(stderr, ERROR("Please create a config file at ~/.config/journalmon/config\n"));
        fprintf(stderr, ERROR("Run 'journalmon --help' for more information.\n"));
        return 1;
    }
    
    print_banner();
    printf(OK("Configuration loaded\n"));
    printf(INFO("   Recipient: %s\n"), config.recipient);
    printf(INFO("   Mailer: %s\n"), config.mailer_path);
    printf(INFO("   Min Priority: %s (≤%d)\n"), get_priority_badge(config.min_priority), config.min_priority);
    if (strlen(config.filters) > 0) {
        printf(INFO("   Filters: %s\n"), config.filters);
    }
    printf(INFO("\nStarting journal monitor...\n\n"));
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Build journalctl command
    char journal_cmd[1024];
    snprintf(journal_cmd, sizeof(journal_cmd),
        "journalctl -f -p %d -o json --no-pager",
        config.min_priority
    );
    
    FILE* journal = popen(journal_cmd, "r");
    if (!journal) {
        fprintf(stderr, ERROR("Failed to start journalctl: %s\n"), strerror(errno));
        return 1;
    }
    
    char line[MAX_LINE];
    int error_count = 0;
    
    while (running && fgets(line, sizeof(line), journal) != NULL) {
        // Parse JSON (simple parsing for key fields)
        char message[MAX_LINE] = {0};
        char syslog_id[256] = {0};
        char unit[256] = {0};
        char timestamp[128] = {0};
        int priority = 3;
        
        // Extract fields (basic JSON parsing)
        char* p;
        if ((p = strstr(line, "\"MESSAGE\":\""))) {
            sscanf(p, "\"MESSAGE\":\"%[^\"]\"", message);
        }
        if ((p = strstr(line, "\"SYSLOG_IDENTIFIER\":\""))) {
            sscanf(p, "\"SYSLOG_IDENTIFIER\":\"%[^\"]\"", syslog_id);
        }
        if ((p = strstr(line, "\"_SYSTEMD_UNIT\":\""))) {
            sscanf(p, "\"_SYSTEMD_UNIT\":\"%[^\"]\"", unit);
        }
        if ((p = strstr(line, "\"PRIORITY\":\""))) {
            char prio_str[4];
            sscanf(p, "\"PRIORITY\":\"%[^\"]\"", prio_str);
            priority = atoi(prio_str);
        }
        if ((p = strstr(line, "\"__REALTIME_TIMESTAMP\":\""))) {
            sscanf(p, "\"__REALTIME_TIMESTAMP\":\"%[^\"]\"", timestamp);
        }
        
        // Skip if no message
        if (strlen(message) == 0) continue;
        
        // Apply filters
        if (strlen(config.filters) > 0) {
            int matches = 0;
            char* filter = strtok(config.filters, ",");
            while (filter) {
                if (strstr(syslog_id, filter) || strstr(unit, filter)) {
                    matches = 1;
                    break;
                }
                filter = strtok(NULL, ",");
            }
            if (!matches) continue;
        }
        
        error_count++;
        
        // Get hostname
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        
        // Format timestamp
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", tm_info);
        
        printf(INFO("[%d] Priority %d: %s - %s\n"), error_count, priority, syslog_id, message);
        
        // Create email
        char subject[512];
        snprintf(subject, sizeof(subject), "[%s] System Alert: %s on %s",
            get_priority_badge(priority), syslog_id, hostname);
        
        char* html = create_html_email(
            hostname,
            strlen(syslog_id) ? syslog_id : "unknown",
            message,
            time_str,
            priority,
            strlen(unit) ? unit : "N/A"
        );
        
        if (html) {
            send_email(subject, html);
            free(html);
        }
        
        // Small delay to avoid spam
        if (config.batch_window > 0) {
            sleep(1);
        }
    }
    
    pclose(journal);
    printf(OK("\nShutdown complete. Monitored %d errors.\n"), error_count);
    
    return 0;
}
