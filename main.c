
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

#define CHECK(r) if (r < 0) {fprintf(stderr, "SD-Bus error at %s: %s\n", __LINE__, strerror(-r)); return r;}

static int method_close(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    printf("Request closed.\r\n");
    return 1;
}

static int signal_response(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    static char dir_path[512];
    uint32_t response;
    int r;

    printf("Response emitted.\r\n");

    /* Read the parameters */
    r = sd_bus_message_read(m, "u", &response);
    if (r < 0)
    {
        fprintf(stderr, "Failed to parse response parameter: %s\n", strerror(-r));
        return r;
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
                printf("URIs:\r\n");
                CHECK(sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, value));
                while (sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &value) > 0)
                {
                    printf(" - %s\r\n", value);
                    strcpy(dir_path, value + 7);
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

    switch (response)
    {
    case 0:
        printf("Success.\r\n");
        printf("Selected dir: %s\r\n", dir_path);
        break;
    case 1:
        printf("User Cancelled.\r\n");
        break;
    default:
        printf("Unknown Cancelled.\r\n");
        break;
    }

    return 1;
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
    const char *path;
    int r;

    /* Connect to the user bus */
    r = sd_bus_open_user(&bus);
    if (r < 0)
    {
        fprintf(stderr, "Failed to connect to bus: %s\n", strerror(-r));
        goto finish;
    }

    /* Open file picker dialog */
    r = sd_bus_call_method(bus,
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
                        //    "handle_token", "s", "/net/test/PortalRequest",
                           "directory", "b", 1
    );

    if (r < 0)
    {
        fprintf(stderr, "Failed to issue method call: %s\n", error.message);
        goto finish;
    }

    /* Parse the response message */
    r = sd_bus_message_read(m, "o", &path);
    if (r < 0)
    {
        fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
        goto finish;
    }
    printf("Queued service job as %s.\n", path);

    r = sd_bus_match_signal(bus, NULL,
                            "org.freedesktop.portal.Desktop",
                            NULL,
                            "org.freedesktop.portal.Request",
                            "Response",
                            signal_response,
                            NULL);
    if (r < 0)
    {
        fprintf(stderr, "Failed to match signal: %s\n", strerror(-r));
        goto finish;
    }

    for (;;)
    {
        /* Process requests */
        sd_bus_message *m = NULL;
        r = sd_bus_process(bus, &m);
        if (r < 0)
        {
            fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
            goto finish;
        }
        if (r > 0) /* we processed a request, try to process another one, right-away */
        {
            printf("Proccessed.\r\n");
            // continue;
        }

        /* Wait for the next request to process */
        r = sd_bus_wait(bus, (uint64_t)-1);
        if (r < 0)
        {
            fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
            goto finish;
        }
    }

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}