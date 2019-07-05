(use-modules (gi)
             (test automake-test-lib))

(automake-test
 (begin
   (typelib-load "Gio" "2.0")
   (let ((app (make-gobject (get-gtype <GApplication>)
                            '(("application-id" . "gi.guile.Example")))))
     (with-object app
       (connect! activate
         (lambda (app)
           (display "Hello, world")
           (newline)
           (with-object app (quit))))
       (run (length (command-line)) (command-line))))))