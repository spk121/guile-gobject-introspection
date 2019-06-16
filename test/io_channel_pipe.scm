(use-modules (gi) (gi glib-2)
             (rnrs bytevectors)
             (test automake-test-lib))

(define SIZ 10)

(define (subbytevector bv start end)
  (let ((bv2 (make-bytevector (- end start))))
    (bytevector-copy! bv start bv2 0 (- end start))
    bv2))

(automake-test

 ;; let's make a pipe to push data through
 (let* ((ports (pipe))
        (in-port (car ports))
        (out-port (cdr ports)))
   (format #t "In Port ~S, Out Port ~S~%" in-port out-port)

   ;; We need the file descriptors if we're going ot make an IOChannel
   (let ((in-fd (port->fdes in-port))
         (out-fd (port->fdes out-port)))
     (format #t "In FD ~S, Out FD ~S~%" in-fd out-fd)
     (let ((channel (IOChannel-unix-new in-fd))

           ;; This is where we'll put the output
           (buf (make-bytevector SIZ 0)))
       (format #t "In IOChannel ~S~%" channel)

       (format #t "Writing 'hello' to Out Port~%")
       (display "hello" out-port)
       (close out-port)

       ;; 
       (let ((status-nbytes (send channel (read-chars buf SIZ))))
         (let ((status (car status-nbytes))
               (nbytes-read (cadr status-nbytes)))
               
           (write (utf8->string buf)) (newline)
           (write status-nbytes) (newline)
           (and (string=?
                 (utf8->string (subbytevector buf 0 5))
                 "hello")
                (equal? 5               ; the number of bytes in 'hello'
                        nbytes-read))))))))
