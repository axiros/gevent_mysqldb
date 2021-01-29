import gevent.socket
from ._exceptions import OperationalError
from .constants.CR import UNKNOWN_HOST
from .constants.CR import CONNECTION_ERROR


def resolve_host(host, port, timeout):
    """This does a async getaddrinfo for 'host' """
    try:
        addr_list = gevent.socket.getaddrinfo(
            host,
            port,
            gevent.socket.AF_UNSPEC,
            gevent.socket.SOCK_STREAM,
            gevent.socket.IPPROTO_TCP)
    except Exception as error:
        msg = "Unknown MySQL server host '%s' (%s)"
        raise OperationalError(UNKNOWN_HOST, msg % (host, error))

    # Do the same as mariadb-connector-c to find the host.
    # Needed, because getaddrinfo could return multiple matches.
    # (IPv4, IPv6, multiple DNS records)

    errors = []
    for result in addr_list:
        family, socket_type, proto, canonname, sockaddr = result

        # Try if the found IP is reachable.
        handle = gevent.socket.socket(family, socket_type, proto)
        handle.settimeout(timeout)
        try:
            handle.connect(sockaddr)
        except Exception as error:
            errors.append((sockaddr, str(error)))
        else:
            handle.close()
            return sockaddr[0]

    msg = "Can't connect to MySQL server on '%s': %s"
    raise OperationalError(CONNECTION_ERROR,  msg % (host, errors))
