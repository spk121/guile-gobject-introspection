(use-modules (gi)
             (rnrs bytevectors)
             (system foreign)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(automake-test
 (let* ([key (string->utf8 "key")]
        [bv (string->utf8 "The quick brown fox jumps over the lazy dog")]
        [bytes-key (bytes:new-take key)]
        [bytes (bytes:new-take bv)]
        [hmac (compute-hmac-for-bytes
               (make <%GChecksumType> 'md5)
               bytes-key
               bytes)])
   (write key) (newline)
   (write bv) (newline)
   (write bytes-key) (newline)
   (write bytes) (newline)
   (write hmac) (newline)
   (string=? hmac "80070713463e7749b90c2dc24911e275")))
