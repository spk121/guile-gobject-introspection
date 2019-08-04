(use-modules (gi)
             (ice-9 hash-table)
             (system foreign)
             (rnrs bytevectors)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

;; FIXME: What the test is intended to do is set up a log handler, and
;; then write to the log.  But the only introspected log writing
;; procedure is 'log-structured-array', and that requires directly
;; setting the fields of a structure.

(define (my-logger domain level message user-data)
  (format #t "LOG DOMAIN ~s, LEVEL ~S, MESSAGE ~S, USER_DATA ~S~%"
          domain level message user-data))

(define (destroy-notify user-data)
  (format #t "DESTROY NOTIFY ~S~%" user-data))

(automake-test
 (let ((ID (log-set-handler #f
                            (list->log-level-flags
                              '(level-warning flag-fatal flag-recursion))
                            my-logger
                            #f
                            destroy-notify)))

   ;; FIXME: here one would write to the log using
   ;; log-structured-array.
   'skipped))
