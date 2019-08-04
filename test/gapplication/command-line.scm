(use-modules (gi) (gi repository)
             (test automake-test-lib)
             (srfi srfi-43))

(unless (false-if-exception (require "Gio" "2.0"))
  (exit EXIT_SKIPPED))

(load-by-name "Gio" "Application")
(load-by-name "Gio" "ApplicationCommandLine")
(load-by-name "Gio" "ApplicationFlags")

(automake-test
 (begin
   (let ((app (make <GApplication>
                #:application-id "gi.guile.Example"
                #:flags '(handles-command-line)))
         (success #f))
     (connect app command-line
              (lambda (app command-line)
                (let ((args (get-arguments command-line)))
                  (vector-for-each (lambda (i world)
                                     (format #t "Hello, ~a~%" world))
                                   args)
                  (quit app)
                  (set! success (vector= string=? args #("hello" "world" "darkness, my old friend")))
                  0)))
     (run app #("hello" "world" "darkness, my old friend"))
     success)))
