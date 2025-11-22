# 🔍 journalmon - System Error Monitor

A beautiful, production-ready systemd journal monitor that emails you when errors occur, with stunning dark-mode HTML emails.

## 🚀 Quick Setup

### 1. Build the Program

```bash
# Compile
gcc -o journalmon journalmon.c -Wall -O2

# Install
sudo cp journalmon /usr/local/bin/
sudo chmod +x /usr/local/bin/journalmon
```

### 2. Create Configuration

```bash
# Create config directory
mkdir -p ~/.config/journalmon

# Create config file
cat > ~/.config/journalmon/config << 'EOF'
# Email recipient for alerts
recipient=admin@example.com

# Path to the mailer binary
mailer_path=/usr/local/bin/mailer

# Minimum priority to monitor (0=emerg, 3=error, 4=warning, 7=debug)
min_priority=3

# Batch window in seconds (to avoid email spam)
batch_window=60

# Optional: Filter specific services (comma-separated)
# filters=nginx,postgresql,sshd
EOF
```

### 3. Create Systemd Service

```bash
sudo tee /etc/systemd/system/journalmon.service > /dev/null << 'EOF'
[Unit]
Description=Journal Error Monitor Service
Documentation=man:journalctl(1)
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/journalmon
Restart=always
RestartSec=10
User=root
StandardOutput=journal
StandardError=journal

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths=/tmp

[Install]
WantedBy=multi-user.target
EOF
```

### 4. Enable and Start

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service
sudo systemctl enable journalmon

# Start service
sudo systemctl start journalmon

# Check status
sudo systemctl status journalmon
```

## 📧 Email Preview

The emails are designed to be **absolutely stunning** with:

- **Dark mode optimized** with gradient backgrounds
- **Priority-coded colors** (red for critical, amber for warnings)
- **Clean, modern design** with proper spacing and typography
- **Monospace code blocks** for error messages
- **Information grid** with icons and badges
- **Quick action commands** for debugging
- **Fully responsive** for mobile and desktop

### Email Features

✨ **Visual Design:**
- Gradient backgrounds with subtle patterns
- Color-coded priority badges (🔴 ERROR, 🟡 WARNING, 🔵 NOTICE)
- Glassmorphism effects with backdrop blur
- Professional shadows and borders
- Emoji icons for better scannability

📋 **Information Displayed:**
- Host name
- Service/Unit name
- Exact timestamp
- Priority level with badge
- Full error message in monospace
- Quick action command to view logs

🎨 **Dark Mode Excellence:**
- Deep navy/black gradient background
- Subtle white borders with low opacity
- Perfect contrast ratios for readability
- Modern glassmorphism aesthetic
- No harsh whites, all colors are muted for eye comfort

## ⚙️ Configuration Options

### Priority Levels

```
0 = EMERGENCY   🔴 (system is unusable)
1 = ALERT       🔴 (action must be taken immediately)
2 = CRITICAL    🔴 (critical conditions)
3 = ERROR       🔴 (error conditions) ← DEFAULT
4 = WARNING     🟡 (warning conditions)
5 = NOTICE      🔵 (normal but significant)
6 = INFO        🟢 (informational messages)
7 = DEBUG       ⚪ (debug-level messages)
```

### Config File Examples

**Monitor only critical errors:**
```
recipient=admin@example.com
mailer_path=/usr/local/bin/mailer
min_priority=2
batch_window=30
```

**Monitor specific services:**
```
recipient=devops@company.com
mailer_path=/usr/local/bin/mailer
min_priority=3
batch_window=60
filters=nginx,postgresql,docker
```

**Aggressive monitoring (all warnings):**
```
recipient=sysadmin@example.com
mailer_path=/usr/local/bin/mailer
min_priority=4
batch_window=120
```

## 🛠️ Usage Examples

### Run Manually (Testing)

```bash
# Run with default config
journalmon

# Run with custom config
journalmon -c /path/to/config

# Check version
journalmon --version

# Show help
journalmon --help
```

### View Logs

```bash
# View journalmon service logs
sudo journalctl -u journalmon -f

# View last 50 lines
sudo journalctl -u journalmon -n 50

# View errors only
sudo journalctl -u journalmon -p err
```

### Test Email Sending

Before enabling the service, test that emails work:

```bash
# Run journalmon manually and trigger an error
journalmon &

# In another terminal, trigger a test error
logger -p err -t test-service "This is a test error message"

# Check if email was sent
# You should see output like:
# 🔔 [1] Priority 3: test-service - This is a test error message
# 📧 Email sent successfully
```

## 🔧 Advanced Configuration

### System-Wide Config

For system-wide deployment:

```bash
sudo mkdir -p /etc/journalmon
sudo cp ~/.config/journalmon/config /etc/journalmon/config
sudo chmod 600 /etc/journalmon/config
```

### Multiple Recipients

To send to multiple recipients, modify the mailer call in the C code or use a mailing list address:

```
recipient=alerts@example.com
```

Then configure `alerts@example.com` as a distribution list in your mail server.

### Rate Limiting

The `batch_window` setting helps prevent email spam:

```
batch_window=60  # Wait 60 seconds between emails
```

### Service-Specific Monitoring

Monitor only specific services:

```
filters=nginx,postgresql,redis,docker,sshd
```

## 🐛 Troubleshooting

### Service Won't Start

```bash
# Check systemd status
sudo systemctl status journalmon

# Check for config errors
journalmon -c ~/.config/journalmon/config

# Check permissions
ls -la /usr/local/bin/journalmon
ls -la ~/.config/journalmon/config
```

### No Emails Received

```bash
# Test mailer directly
mailer --to "test@example.com" --subject "Test" --body "Hello"

# Check journalmon logs
sudo journalctl -u journalmon -n 100

# Verify config
cat ~/.config/journalmon/config
```

### Too Many Emails

Increase `batch_window` or raise `min_priority`:

```
min_priority=2  # Only critical and above
batch_window=300  # 5 minute delay between emails
```

### Permission Denied

The service needs root to read system journals:

```bash
# Ensure service runs as root
sudo systemctl edit journalmon

# Add:
[Service]
User=root
```

## 📊 Monitoring the Monitor

### Check Service Health

```bash
# Is it running?
sudo systemctl is-active journalmon

# How long has it been running?
sudo systemctl status journalmon

# Resource usage
sudo systemctl show journalmon --property=MemoryCurrent
```

### View Statistics

```bash
# Count emails sent (from logs)
sudo journalctl -u journalmon | grep -c "Email sent successfully"

# View error types
sudo journalctl -u journalmon | grep "Priority" | awk '{print $5}' | sort | uniq -c
```

## 🎨 Email Customization

To modify the email design, edit the `create_html_email()` function in `journalmon.c`:

**Color scheme variables:**
```c
// Priority colors
case 3: return "#ef4444"; // Red for errors
case 4: return "#f59e0b"; // Amber for warnings
```

**Gradient backgrounds:**
```c
background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
```

**Badge styling:**
```c
background: linear-gradient(135deg, %s 0%%, %s 100%%);
```

## 🔒 Security Considerations

1. **Config file permissions:**
   ```bash
   chmod 600 ~/.config/journalmon/config
   ```

2. **Mailer credentials:** Store in mailer's config, not journalmon's

3. **Service hardening:** The systemd service includes:
   - `NoNewPrivileges=true`
   - `PrivateTmp=true`
   - `ProtectSystem=strict`
   - `ProtectHome=read-only`

4. **Log rotation:** Systemd handles this automatically

## 🌟 Features

✅ Real-time monitoring of systemd journal  
✅ Beautiful, dark-mode HTML emails  
✅ Priority-based filtering  
✅ Service-specific filtering  
✅ Batch window to prevent spam  
✅ Automatic error parsing  
✅ Hostname and timestamp tracking  
✅ Quick action commands in emails  
✅ Systemd service integration  
✅ Graceful shutdown handling  
✅ Comprehensive error messages  
✅ Production-ready and tested  

## 📄 License

This is a tool for hobbyist. Use it to keep your systems healthy! 🚀
