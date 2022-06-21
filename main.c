
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

static char path[1024];
static volatile int path_status;

#define CHECK(r) if (r < 0) {fprintf(stderr, "SD-Bus error at %s: %s\n", __LINE__, strerror(-r)); path_status = -1; goto finish;}

static int method_close(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    printf("Request closed.\r\n");
    return 1;
}

static int signal_response(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    uint32_t response;
    int r;

    /* Read the parameters */
    CHECK(sd_bus_message_read(m, "u", &response));
    if (response > 0)
    {
        path_status = -2;
        return 1;
    }

    CHECK(sd_bus_message_enter_container(m, 'a', "{sv}"));
    while (sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0)
    {
        const char *key;
        const char *value;
        char type;

        CHECK(sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &key));
        CHECK(sd_bus_message_peek_type(m, &type, &value));

        if (type == SD_BUS_TYPE_VARIANT)
        {
            CHECK(sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, value));
            CHECK(sd_bus_message_peek_type(m, &type, &value));
            if (type == SD_BUS_TYPE_ARRAY && strstr(key, "uris") != NULL)
            {
                CHECK(sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, value));
                while (sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &value) > 0)
                {
                    strcpy(path, value + 7);
                    path_status = 1;
                }
                CHECK(sd_bus_message_exit_container(m));
            }
            else
            {
                sd_bus_message_skip(m, NULL);
            }
            CHECK(sd_bus_message_exit_container(m));
        }
        CHECK(sd_bus_message_exit_container(m));
    }
    sd_bus_message_exit_container(m);

finish:
    return r;
}

static const sd_bus_vtable test_object_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Close", "", "", method_close, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Response", "ua{sv}", 0),
    SD_BUS_VTABLE_END
};

int main(int argc, char *argv[])
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    const char *proc_path;
    int r;

    /* Connect to the user bus */
    CHECK(sd_bus_open_user(&bus));

    /* Open file picker dialog */
    CHECK(sd_bus_call_method(bus,
                           "org.freedesktop.portal.Desktop",             /* service to contact */
                           "/org/freedesktop/portal/desktop",            /* object path */
                           "org.freedesktop.portal.FileChooser",         /* interface name */
                           "OpenFile",                                   /* method name */
                           &error,                                       /* object to return error in */
                           &m,                                           /* return message on success */
                           "ssa{sv}",                                    /* input signature */
                           "",                                           /* parent_window */
                           "pick file",                                  /* title */
                           1,                                            /* options */
                           "directory", "b", 1));

    /* Parse the response message */
    CHECK(sd_bus_message_read(m, "o", &proc_path));

    // Register signal event
    CHECK(sd_bus_match_signal(bus, NULL,
                            "org.freedesktop.portal.Desktop",
                            NULL,
                            "org.freedesktop.portal.Request",
                            "Response",
                            signal_response,
                            NULL));

    while (path_status == 0)
    {
        /* Process requests */
        sd_bus_message *m = NULL;
        CHECK(sd_bus_process(bus, &m));
        if (r > 0) continue;

        /* Wait for the next request to process */
        CHECK(sd_bus_wait(bus, (uint64_t)50000));
    }

    if (path_status > 0)
    {
        printf("%s\r\n", path);
    }
    else if (path_status == -2)
    {
        printf("Canceled\r\n");
    }

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}