# Test code

```go
package main

import (
	"os/exec"
	"time"
)

func main() {
	for {
		// Info-level message (priority 6)
		exec.Command("logger", "-p", "user.info", "info message").Run()

		// Error-level message (priority 3)
		exec.Command("logger", "-p", "user.err", "error level message").Run()

		// Critical-level message (priority 2)
		exec.Command("logger", "-p", "user.crit", "critical level message").Run()

		time.Sleep(3 * time.Second)
	}
}
```

This:
* writes to `stderr` using `os.Stderr`
* pauses 3 seconds between prints
* runs indefinitely


# Systemd for Test code

```ini
[Unit]
Description=Test message generator
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/testmsg
Restart=always
RestartSec=2
StandardError=journal
StandardOutput=journal

[Install]
WantedBy=multi-user.target
```

### Notes

* Put it at:
  **/etc/systemd/system/testmsg.service**
* Replace `/usr/local/bin/testmsg` with the actual path to your compiled Go binary.
* Enable and start it:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now testmsg.service
```
