(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GObject" "2.0"))

(define my-signal
  (make-signal
   #:name "my-signal"
   #:flags (make <%GSignalFlags> '(detailed))
   #:return-type G_TYPE_NONE))

(define <TestParam>
  ((@ (gi) register-type)
   "TestParam"
   <GObject>
   '()
   (list my-signal)))

(automake-test
 (let ((object (make <TestParam>))
       (success #f)
       (fail #f))
   (connect object my-signal 'wrong-detail (lambda (obj) (set! fail #t)))
   (connect object my-signal 'detail (lambda (obj) (set! success #t)))
   (my-signal object 'detail)
   (and success (not fail))))
