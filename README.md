# autohup
automaticly sends SIGHUP to a process when a file changes.

## usage

```
usage: ./autohup [-s signal] [-e script] [-v] [path ...] -- command [argument ...]
       ./autohup -l
```

## exit code

`autohup` always returns 255 if an error occurs internally. If the program
defined with `command` exits, its error code is used.

## example

This example watches for changes in `/etc/daemon.d` and sends `SIGHUP` when
inotify registers a change in that directory.
```
$ autohup /etc/daemon.d -- daemon
```

The second example watches multiple directories and sends `SIGTERM` instead.
Beware that the `SIG...` prefix is optional and the option is case insensite.
```
$ autohup -s term /etc/daemon.d /var/lib/daemon -- daemon
```

Optionally an event script can be defined, that runs right before an event is
sent
```
$ autohup -e 'echo preparing...' /etc/daemon.d -- daemon
```

To list all signals autohup can send use this command:
```
$ autohup -l
```

Signals can also be defined as numbers
```
$ autohup /etc/daemon.d -s 10 -- daemon # 10 = SIGUSR2
```
