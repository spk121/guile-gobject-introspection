(use-modules (lib)
	     (ice-9 eval-string)
	     (srfi srfi-9 gnu))

(set-record-type-printer! <GObject> gobject-printer)
(set-record-type-printer! <GBox> gbox-printer)

(let* ((rpt (make-count-reporter))
       (counter-proc (car rpt))
       (results-proc (cadr rpt))
       (ctx #f)
       (source #f)
       (id 0))
  (register-reporter counter-proc)
  (register-reporter full-reporter)
  
  (with-test-prefix
   "load typelib"
   
   (pass-if "load GLib 2.0 repository"
	    (eval-string (gi-load-repository "GLib" "2.0"))
	    #t))

  (with-test-prefix
   "main context"

   (pass-if "main-context-new returns a context"
	    (set! ctx (main-context-new))
	    (gbox? ctx))
   
   (pass-if "main context is not pending"
	    (not (main-context-pending? ctx)))

   (pass-if "an iteration of the main loop doesn't dispatch events"
	    (not (main-context-iteration? ctx #f))))
  
  (with-test-prefix
   "unload typelib"
   
   (pass-if "unload repositories"
	    (gi-unload-repositories)
	    #t))
  
  (print-counts (results-proc))
  ;;(exit-value (results-proc))
  )
