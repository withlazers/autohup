# autohup

automaticly sends SIGHUP to a process when a file changes. This is handy for
containers that need to reload configuration as they change.

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
$ autohup -e 'cat /etc/daemon.d/* > /etc/daemon.conf' /etc/daemon.d -- daemon
```

To list all signals autohup can send use this command:
```
$ autohup -l
```

Signals can also be defined as numbers
```
$ autohup -s 10 /etc/daemon.d -- daemon # 10 = SIGUSR2
```

## bugs

`autohup` has a debouncing mechanism which sends the signal after 1 second with
no file change. This means that if you're constantly changing files, the signal
will never be sent.
