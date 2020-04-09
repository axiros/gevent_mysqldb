#include <type_traits>
#include <utility>
#include "mysql.h"

/* All helper functions are prefixed with async_ext */

static MYSQL* async_ext_get_mysql(MYSQL* c) { return c; }
static MYSQL* async_ext_get_mysql(MYSQL_RES* res) {return res->handle; }

static PyObject* gevent_wait_read;
static PyObject* gevent_wait_write;
static PyObject* gevent_socket_timeout;


static int
import_gevent_objects()
{
    PyObject* mod = NULL;
    gevent_wait_read = NULL;
    gevent_wait_write = NULL;

    if ((mod = PyImport_ImportModule("gevent.socket")) == NULL) {
        goto error;
    }

    gevent_wait_read = PyObject_GetAttrString(mod, "wait_read");
    if (gevent_wait_read == NULL) {
        goto error;
    }

    gevent_wait_write = PyObject_GetAttrString(mod, "wait_write");
    if (gevent_wait_write == NULL) {
        goto error;
    }

    gevent_socket_timeout = PyObject_GetAttrString(mod, "timeout");
    if (gevent_socket_timeout == NULL) {
        goto error;
    }

    Py_DECREF(mod);
    return 0;

error:
    Py_XDECREF(mod);
    Py_XDECREF(gevent_wait_read);
    Py_XDECREF(gevent_wait_write);
    Py_XDECREF(gevent_socket_timeout);
    return -1;
}

/* build the timeout given to gevent_ wait/read */
static PyObject*
async_ext_build_py_timeout(int status, MYSQL* conn)
{
    double timeout = -1;
    if (status & MYSQL_WAIT_TIMEOUT) {
        timeout = mysql_get_timeout_value_ms(conn) / 1000.0;
    }

    if (timeout == -1) {
        Py_RETURN_NONE;
    } else {
        return PyFloat_FromDouble(timeout);
    }
}

/* Any python error in 'gevent_wait' is written to stderr.
 * The error is signaled to the mysql library via 'MYSQL_WAIT_TIMEOUT.'
 * 'MYSQL_WAIT_EXCEPT' is not used by libmariadb.
 */
static int
async_ext_report_python_error()
{
    if (PyErr_Occurred() && !PyErr_ExceptionMatches(gevent_socket_timeout)) {
        PyErr_PrintEx(0);
    }
    return MYSQL_WAIT_TIMEOUT;
}

/* Here we do the waiting on the socket for read/write via gevent. */
static int
async_ext_gevent_wait(int status, MYSQL *conn)
{
    PyObject* result = NULL;

    PyObject* py_timeout = async_ext_build_py_timeout(status, conn);
    if (py_timeout == NULL) {
        return async_ext_report_python_error();
    }

    PyObject* py_socket = PyLong_FromLong(mysql_get_socket(conn));
    if (py_socket == NULL) {
        Py_DECREF(py_timeout);
        return async_ext_report_python_error();
    }

    if (status & MYSQL_WAIT_READ) {
        status = MYSQL_WAIT_READ;
        result = PyObject_CallFunctionObjArgs(
                gevent_wait_read,
                py_socket,
                py_timeout,
                NULL);

    } else if (status & MYSQL_WAIT_WRITE) {
        status = MYSQL_WAIT_WRITE;
        result = PyObject_CallFunctionObjArgs(
            gevent_wait_write,
            py_socket,
            py_timeout,
            NULL);
    }

    Py_DECREF(py_socket);
    Py_DECREF(py_timeout);

    if (result == NULL) {
        return async_ext_report_python_error();
    } else {
        Py_DECREF(result);
        return status;
    }
}

template <typename R, typename S, typename C, typename H, typename... Args>
static auto
async_ext_do_async(S start_func, C cont_func, H handle, Args&&... args)
{

    int status;
    R result;

    status = start_func(&result, handle, std::forward<Args>(args)...);
    while (status) {
        status = async_ext_gevent_wait(status, async_ext_get_mysql(handle));
        status = cont_func(&result, handle, status);
    }

    return result;
}

#define MAKE_ASYNC(orig_func) \
template <typename... Args> \
static auto gevent_async_##orig_func(Args&&... args) { \
    using R = std::result_of_t<decltype(&orig_func)(Args...)>; \
    return async_ext_do_async<R> ( \
        orig_func##_start, \
        orig_func##_cont , \
        std::forward<Args>(args)... \
    ); \
}

/* https://mariadb.com/kb/en/non-blocking-api-reference/ */

MAKE_ASYNC(mysql_store_result);
MAKE_ASYNC(mysql_real_connect);
MAKE_ASYNC(mysql_dump_debug_info);
MAKE_ASYNC(mysql_autocommit);
MAKE_ASYNC(mysql_commit);
MAKE_ASYNC(mysql_rollback);
MAKE_ASYNC(mysql_next_result);
MAKE_ASYNC(mysql_set_server_option);
MAKE_ASYNC(mysql_fetch_row);
MAKE_ASYNC(mysql_change_user);
MAKE_ASYNC(mysql_set_character_set);
MAKE_ASYNC(mysql_kill);
MAKE_ASYNC(mysql_ping);
MAKE_ASYNC(mysql_real_query);
MAKE_ASYNC(mysql_send_query);
MAKE_ASYNC(mysql_read_query_result);
MAKE_ASYNC(mysql_select_db);
MAKE_ASYNC(mysql_shutdown);

/* For illustration purpose:
 * This is the FINAL thing MAKE_ASYNC(mysql_store_result) produces:
 *
 * MYSQL_RES*
 * gevent_async_mysql_store_result(MYSQL* conn) {
 *
 *      int status;
 *      MYSQL_RES* result;
 *
 *      status = mysql_store_result_start(&result, conn);
 *      while (status) {
 *          status = gevent_wait(status, conn);
 *          status = mysql_store_result_cont(&result, conn, status);
 *      }
 *      
 *      return result;
 * }
 */


/* Those function cannot be generated with MAKE_ASYNC because:
 * - having NO return value => 'void' as return value.
 * - having 'const char*' as return value.
 */
static void
gevent_async_mysql_close(MYSQL* conn)
{
    int status;
    status = mysql_close_start(conn);
    while (status) {
        status = async_ext_gevent_wait(status, conn);
        status = mysql_close_cont(conn, status);
    }
}

static auto
gevent_async_mysql_stat(MYSQL* conn) {
    return async_ext_do_async<const char*>(
        mysql_stat_start,
        mysql_stat_cont,
        conn
    );
}
