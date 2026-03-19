#include "xim_server.h"
#include "engine.h"
#include "candidate_window.h"
#include "util.h"

#include <signal.h>
#include <stdlib.h>
#include <locale.h>

static bsdjp_server_t *g_server = NULL;

static void signal_handler(int sig) {
    (void)sig;
    if (g_server)
        xim_server_stop(g_server);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    setlocale(LC_ALL, "");

    bsdjp_log("BSDJP - FreeBSD Japanese Input Method v0.1.0");

    if (engine_global_init() != 0) {
        bsdjp_error("Failed to initialize conversion engine");
        return 1;
    }

    g_server = xim_server_create();
    if (!g_server) {
        bsdjp_error("Failed to allocate server");
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (xim_server_init(g_server) != 0) {
        bsdjp_error("Failed to initialize XIM server");
        xim_server_destroy(g_server);
        engine_global_cleanup();
        return 1;
    }

    xim_server_run(g_server);
    xim_server_destroy(g_server);
    engine_global_cleanup();

    bsdjp_log("Exiting normally");
    return 0;
}
