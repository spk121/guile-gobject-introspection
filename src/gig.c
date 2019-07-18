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

#include <time.h>
#include <glib-object.h>
#include <glib.h>
#include <girepository.h>
#include <libguile.h>
#include "gig_object.h"
#include "gig_value.h"
#include "gig_signal.h"
#include "gig_argument.h"
#include "gig_typelib.h"
#include "gig_type.h"
#include "gig_callback.h"
#include "gig_function.h"
#include "gig_constant.h"
#include "gig_flag.h"

#ifdef _WIN32
static const gint _win32 = TRUE;
#else
static const gint _win32 = FALSE;
#endif

#ifdef ENABLE_GCOV
void __gcov_reset(void);
void __gcov_dump(void);
#endif

#ifdef MTRACE
#include <mcheck.h>
#endif

static void
gig_log_handler(const gchar *log_domain,
                GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    time_t timer;
    gchar buffer[26];
    struct tm *tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // Opening and closing files as append in Win32 is noticeably slow.
    if (log_level == G_LOG_LEVEL_DEBUG && !_win32) {
        FILE *fp = fopen("gir-debug-log.xt", "at");
        fprintf(fp, "%s: %s %d %s\n", buffer, log_domain, log_level, message);
        fclose(fp);
    }
    else
        fprintf(stderr, "%s: %s %d %s\n", buffer, log_domain, log_level, message);
    fflush(stderr);
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
gig_init(void)
{
#ifdef MTRACE
    mtrace();
#endif
#if 0
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                      | G_LOG_FLAG_RECURSION, gig_log_handler, NULL);
    g_log_set_handler("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                      | G_LOG_FLAG_RECURSION, gig_log_handler, NULL);
#endif
    g_debug("Begin libguile-gir initialization");
    gig_init_types();
    gig_init_typelib();
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

gint
main(gint argc, gchar **argv)
{
    scm_init_guile();

    gig_init();
    scm_shell(argc, argv);
    return 0;
}