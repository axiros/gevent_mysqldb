# gevent_mysqldb

This is a fork of [mysqlclient](https://github.com/PyMySQL/mysqlclient-python) to support gevent.
The purpose is to get the speed of a mysql C-library within a gevent application,
but without the need to use a thread-pool.

This is achieved by modifying `mysql.c` to use the
[non blocking API](https://mariadb.com/kb/en/using-the-non-blocking-library/) of `libmariadb`.
This API allows to customize the waiting for read/write events on a socket.
*gevent_mysql* uses `gevent.socket.wait_read` and `gevent.socket.wait_write` to do so.


## Known Limitations

* The destructor for a `_mysql.Connection` does not happen in a non-blocking way
  when the connection is still open. In that case the destructor calls implicitly
  `mysql_close()` wich cannot be done in gevent friendly way.
  Since switching a greenlet in the code path of garbage collection
  is dangerous. Image the garbage collection is run by the *hub* greenlet. Then
  the *hub* would switch to himself, which is not possible.
  Recommendation: Call `conn.close()` always explicitly.

* The destructor of `_mysql.result` could block other greenlets under the following circumstances.
    * The result object was acquired via `conn.use_result()`.
    * Not all rows from the result have been read by the application.

  `conn.use_result()` gives a *lazy* result back. In case the application
  did **not** consume all rows from this result the mysql-driver has to discard the remaining
  rows. This discard (and all the involved network IO) does not happen in a non-blocking
  way. Recommendation is to use `conn.store_result()`.
  This applies also for `MySQLdb.SSCursor` and `MySQLdb.CursorUseResultMixIn`,
  because the use `conn.use_result()` under the hood.

# mysqlclient
This project is a fork of [MySQLdb1](https://github.com/farcepest/MySQLdb1).
This project adds Python 3 support and fixed many bugs.

* PyPI: https://pypi.org/project/mysqlclient/
* GitHub: https://github.com/PyMySQL/mysqlclient


## Support

**Do Not use Github Issue Tracker to ask help.  OSS Maintainer is not free tech support**

When your question looks relating to Python rather than MySQL:

* Python mailing list [python-list](https://mail.python.org/mailman/listinfo/python-list)
* Slack [pythondev.slack.com](https://pyslackers.com/web/slack)

Or when you have question about MySQL:

* [MySQL Community on Slack](https://lefred.be/mysql-community-on-slack/)


## Install

### Windows

*gevent_mysqldb* hasn't been tested/build on windows yet.

### macOS (Homebrew)

*gevent_mysqldb* hasn't been tested/build on macOS yet.

### Linux

**Note that this is a basic step.  I can not support complete step for build for all
environment.  If you can see some error, you should fix it by yourself, or ask for
support in some user forum.  Don't file a issue on the issue tracker.**

You may need to install the Python 3 and [libmariadb](https://downloads.mariadb.org/connector-c/) development headers and libraries

#### Debian / Ubuntu

`sudo apt-get install python3-setuptools python3-dev build-essential python3-gevent libmariadb-dev-compat`

#### Fedora

`sudo dnf install python3-setuptools python3-devel python3-gevent gcc-c++ mariadb-connector-c-devel`

### RedHat 8 / CentOS 8

`sudo yum install python3-setuptools python3-devel python3-gevent gcc-c++ mariadb-connector-c-devel mariadb-devel`


Then you can install *gevent_mysqldb* via pip now:

```
pip3 install git+https://github.com/axiros/gevent_mysqldb.git

```

### Customize build (POSIX)

mysqlclient uses `mysql_config` or `mariadb_config` by default for finding
compiler/linker flags.

You can use `MYSQLCLIENT_CFLAGS` and `MYSQLCLIENT_LDFLAGS` environment
variables to customize compiler/linker options.

```
$ export MYSQLCLIENT_CFLAGS=`pkg-config mysqlclient --cflags`
$ export MYSQLCLIENT_LDFLAGS=`pkg-config mysqlclient --libs`
$ pip install mysqlclient
```

### Documentation

Documentation is hosted on [Read The Docs](https://mysqlclient.readthedocs.io/)
