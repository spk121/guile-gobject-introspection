(use-modules (gi) (gi glib-2)
             (test automake-test-lib))

(automake-test
 (let ((mainloop (MainLoop-new #f #t)))
   (send mainloop (quit))
   (not (send mainloop (is-running?)))))
