(use-modules (gi)
             (rnrs bytevectors)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(automake-test
 (let ((cksum (checksum:new (make <%GChecksumType> 'sha256)))
       (bv (string->utf8 "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n")))
   (write cksum) (newline)
   (write bv) (newline)
   (update cksum bv)
   (let ((output (get-string cksum)))
     (write output) (newline)
     (string=?
      output
      "a06b168d8e72c069aa3cc58d64b92a300f9f82127facb3219855053e49a4ecbe"))))
