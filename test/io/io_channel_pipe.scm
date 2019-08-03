(use-modules (gi) (gi util)
             (rnrs bytevectors)
             (ice-9 receive)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

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
     (let ((channel (iochannel:unix-new in-fd))

           ;; This is where we'll put the output
           (buf (make-bytevector SIZ 0)))
       (format #t "In IOChannel ~S~%" channel)

       (format #t "Writing 'hello' to Out Port~%")
       (display "hello" out-port)
       (close out-port)

       ;;
       (receive (status nbytes-read)
           (read-chars channel buf)
         (write (utf8->string buf)) (newline)
           (write (list status nbytes-read)) (newline)
           (and (= status (enum->number <%GIOStatus> 'normal))
                (string=?
                 (utf8->string (subbytevector buf 0 5))
                 "hello")
                (equal? 5               ; the number of bytes in 'hello'
                        nbytes-read)))))))
