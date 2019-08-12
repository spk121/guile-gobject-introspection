// Copyright (C) 2018, 2019 Michael L. Gran

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <girepository.h>
#include <glib-object.h>
#include <glib.h>
#include <libguile.h>
#include <stdio.h>
#include <time.h>
#include <threads.h>
#include "gig_argument.h"
#include "gig_callback.h"
#include "gig_constant.h"
#include "gig_flag.h"
#include "gig_function.h"
#include "gig_object.h"
#include "gig_signal.h"
#include "gig_type.h"
#include "gig_util.h"
#include "gig_value.h"

#ifdef ENABLE_GCOV
void __gcov_reset(void);
void __gcov_dump(void);
#endif

#ifdef MTRACE
#include <mcheck.h>
#endif

thread_local int logger_initialized = 0;

static const GLogField *
field_ref(const gchar *needle, const GLogField *fields, gsize n_fields)
{
    for (gsize i = 0; i < n_fields; i++)
        if (!(strcmp(fields[i].key, needle)))
            return fields + i;
    return NULL;
}

static gboolean
is_enabled(GLogLevelFlags flags, const GLogField *domain)
{
    const gchar *domains = g_getenv("G_MESSAGES_DEBUG");
    if (domains == NULL)
        return FALSE;
    if (!strcmp(domains, "all"))
        return TRUE;
    if (domain && domain->value && strstr(domains, domain->value))
        return TRUE;
    return FALSE;
}

static GLogWriterOutput
gig_log_writer(GLogLevelFlags flags, const GLogField *fields, gsize n_fields, gpointer user_data)
{
    const GLogField *message, domain;

    const gchar *prefix, *color;
    if (!logger_initialized) {
        scm_init_guile();
        logger_initialized = 1;
    }
    switch (flags & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
        color = "\033[1;31m%s\033[0m";
        prefix = "ERROR";
        break;
    case G_LOG_LEVEL_CRITICAL:
        color = "\033[1;35m%s\033[0m";
        prefix = "CRITICAL";
        break;
    case G_LOG_LEVEL_WARNING:
        color = "\033[1;33m%s\033[0m";
        prefix = "WARNING";
        break;
    case G_LOG_LEVEL_MESSAGE:
        color = "\033[1;32m%s\033[0m";
        prefix = "MESSAGE";
        break;
    case G_LOG_LEVEL_INFO:
        color = "\033[1;32m%s\033[0m";
        if (!is_enabled(flags, field_ref("GLIB_DOMAIN", fields, n_fields)))
            return G_LOG_WRITER_HANDLED;
        prefix = "INFO";
        break;
    case G_LOG_LEVEL_DEBUG:
        color = "\033[1;32m%s\033[0m";
        if (!is_enabled(flags, field_ref("GLIB_DOMAIN", fields, n_fields)))
            return G_LOG_WRITER_HANDLED;
        prefix = "DEBUG";
        break;
    }

    message = field_ref("MESSAGE", fields, n_fields);
    g_assert(message != NULL);

    SCM port = scm_current_error_port();

    if (SCM_OPFPORTP(port)) {
        gint fd = scm_to_int(scm_fileno(port));
        gchar *colored_prefix =
            g_strdup_printf(g_log_writer_supports_color(fd) ? color : "%s", prefix);
        dprintf(fd, "%s: %s\n", colored_prefix, (const gchar *)message->value);
        g_free(colored_prefix);
    }
    else
        scm_printf(port, "%s: %s\n", prefix, (const gchar *)message->value);

    return G_LOG_WRITER_HANDLED;
}

#ifdef ENABLE_GCOV
static SCM
scm_gcov_reset(void)
{
    __gcov_reset();
    return SCM_UNSPECIFIED;
}


static SCM
scm_gcov_dump(void)
{
    __gcov_dump();
    return SCM_UNSPECIFIED;
}
#endif

void
gig_init_logging()
{
    g_log_set_writer_func(gig_log_writer, NULL, NULL);
}

void
gig_init(void)
{
#ifdef MTRACE
    mtrace();
#endif
    g_debug("Begin libguile-gir initialization");
    gig_init_constant();
    gig_init_flag();
    gig_init_argument();
    gig_init_signal();
    gig_init_callback();
    gig_init_function();
    g_debug("End libguile-gir initialization");

#ifdef ENABLE_GCOV
    scm_c_define_gsubr("gcov-reset", 0, 0, 0, scm_gcov_reset);
    scm_c_define_gsubr("gcov-dump", 0, 0, 0, scm_gcov_dump);
#endif
}

#ifdef STANDALONE
gint
main(gint argc, gchar **argv)
{
    scm_init_guile();

    gig_init();
    scm_shell(argc, argv);
    return 0;
}
#endif
