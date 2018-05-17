#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>

#include <dbus/dbus.h>

#ifdef __cplusplus
#undef DBUS_ERROR_INIT
#define DBUS_ERROR_INIT { nullptr, nullptr, TRUE, 0, 0, 0, 0, nullptr }
#endif


//typedef enum {
//    BINARY_MODE_NOT,
//    BINARY_MODE_RAW,
//    BINARY_MODE_PCAP
//} BinaryMode;


[[noreturn]] static void tool_oom(const char *where)
{
    qCritical() << "Out of memoory:" << where;
    ::exit(100);
}


static dbus_bool_t become_monitor (DBusConnection *connection)
{
    DBusError error = DBUS_ERROR_INIT;
    DBusMessage *m;
    DBusMessage *r;
    dbus_uint32_t zero = 0;
    DBusMessageIter appender, array_appender;

    m = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_MONITORING,
                                     "BecomeMonitor");

    if (m == nullptr) {
        tool_oom ("becoming a monitor");
    }

    dbus_message_iter_init_append(m, &appender);

    if (!dbus_message_iter_open_container(&appender, DBUS_TYPE_ARRAY, "s", &array_appender)) {
        tool_oom ("opening string array");
    }

    if (!dbus_message_iter_close_container(&appender, &array_appender) ||
            !dbus_message_iter_append_basic(&appender, DBUS_TYPE_UINT32, &zero)) {
        tool_oom ("finishing arguments");
    }

    r = dbus_connection_send_with_reply_and_block(connection, m, -1, &error);

    if (r != nullptr) {
        dbus_message_unref(r);
    } else if (dbus_error_has_name(&error, DBUS_ERROR_UNKNOWN_INTERFACE)) {
        qWarning() << "qdbusmonitor: unable to enable new-style monitoring, "
                      "your dbus-daemon is too old. Falling back to eavesdropping.";
        dbus_error_free(&error);
    } else {
        qWarning() << "qdbusmonitor: unable to enable new-style monitoring: "
                   << error.name << ": " << error.message
                   << ". Falling back to eavesdropping.";
        dbus_error_free(&error);
    }

    dbus_message_unref (m);

    return (r != nullptr);
}


static DBusHandlerResult monitor_filter_func (
        DBusConnection     *connection,
        DBusMessage        *message,
        void               *user_data)
{
    Q_UNUSED(connection)
    Q_UNUSED(user_data)

    // TODO: print_message (message, FALSE, sec, usec);

    qDebug() << "DBus message received by filter!";

    if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
        // exit(0);
        QCoreApplication::quit();
    }

    /* Monitors must not allow libdbus to reply to messages, so we eat
     * the message. See bug 1719.
     */
    return DBUS_HANDLER_RESULT_HANDLED;
}


int main(int argc, char *argv[])
{
    DBusConnection *dconnection = nullptr;
    DBusError derror = DBUS_ERROR_INIT;
    DBusBusType type = DBUS_BUS_SESSION;
    DBusHandleMessageFunction filter_func = monitor_filter_func;

    dbus_error_init(&derror);
    dconnection = dbus_bus_get(type, &derror);
    if (!dconnection) {
        qDebug() << "Failed to open dbus connction" << derror.message;
        dbus_error_free(&derror);
        return 1;
    }

    /* Receive o.fd.Peer messages as normal messages, rather than having
     * libdbus handle them internally, which is the wrong thing for
     * a monitor */
    dbus_connection_set_route_peer_messages (dconnection, TRUE);

    if (!dbus_connection_add_filter(dconnection, filter_func, nullptr, nullptr)) {
        qDebug() << "Couldn't add filter!";
        return 1;
    }

    if (!become_monitor(dconnection)) {
        // hack for old dbus server
        dbus_bus_add_match(dconnection, "eavesdrop=true", &derror);
        if (dbus_error_is_set(&derror)) {
            dbus_error_free(&derror);
            dbus_bus_add_match(dconnection, "", &derror);
            if (dbus_error_is_set(&derror)) {
                qWarning() << "Falling back to eavesdropping failed!";
                qWarning() << "Error: " << derror.message;
                ::exit(2);
            }
        }
    }

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    app.setQuitOnLastWindowClosed(true);
    bool should_exit = false;
    QObject::connect(&engine, &QQmlEngine::quit, [&should_exit] () {
        qDebug() << "should_exit!";
        should_exit = true;
    });

    // return app.exec(); // nope
    while (dbus_connection_read_write_dispatch(dconnection, 100)) {
        QCoreApplication::processEvents();
        if (should_exit) {
            break;
        }
    }
    return 0;
}
