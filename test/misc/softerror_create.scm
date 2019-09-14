(use-modules (gi)
             (test automake-test-lib))

(automake-test
 (catch 'wrong-type-arg
   (lambda ()
     (make <GObject> <standard-vtable>)
     EXIT_FAILURE)
   (lambda args EXIT_SUCCESS)))