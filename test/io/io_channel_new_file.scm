(use-modules (gi) (gi util)
             (rnrs bytevectors)
             (ice-9 receive)
             (test automake-test-lib))

(push-duplicate-handler! 'merge-generics)
(typelib-require ("GLib" "2.0"))

(define SIZ 10)

(define (subbytevector bv start end)
  (let ((bv2 (make-bytevector (- end start))))
    (bytevector-copy! bv start bv2 0 (- end start))
    bv2))

(define (err-text status)
  (cond
   ((= status (enum->number <%GIOStatus> 'error))
    "error")
   ((= status (enum->number <%GIOStatus> 'normal))
    "normal")
   ((= status (enum->number <%GIOStatus> 'eof))
    "eof")
   ((= status (enum->number <%GIOStatus> 'again))
    "again")))

(automake-test
 (begin
   ;; Let's make a channel that writes to a file.
   (let ((channel (iochannel:new-file "tmp.txt" "w")))
     (let ((result (write-chars channel (string->utf8 "foobar") 6)))
       (shutdown channel #t)))

   ;; Now, make a channel that reads from that file.
   (let ((channel (iochannel:new-file "tmp.txt" "r"))
         (buf (make-bytevector SIZ 0)))

     ;; Read as much as we can.
     (receive (status nbytes)
         (read-chars channel buf)
       (let ((bv (subbytevector buf 0 nbytes)))
           (format #t "Output bytevector contents: ~S~%" bv)
           (format #t "Output bytevector as UTF8: ~S~%" (utf8->string bv))
           (format #t "Bytes read: ~S~%" nbytes)
           (format #t "Error code: ~S ~S~%" status (err-text status))
           (and (string=?
                 (utf8->string bv)
                 "foobar")
                (equal? 6               ; the number of bytes in 'foobar'
                        nbytes)))))))
