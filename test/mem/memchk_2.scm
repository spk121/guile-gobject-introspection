(use-modules (gi) (gi repository)
             (test automake-test-lib))

(automake-test
 (begin
   (format #t "Running a GC after loading Glib~%")
   (typelib->module (current-module) "GLib" "2.0")
   (gc)
   #t))