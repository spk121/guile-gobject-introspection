(use-modules (gi)
             (rnrs bytevectors)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(automake-test
 (let* ([bv (string->utf8 "The quick brown fox jumps over the lazy dog")]
        [len (bytevector-length bv)]
        [hmac (compute-hmac-for-data
               (symbol->checksum-type 'md5)
               (string->utf8 "key")
               bv)])
   (write bv) (newline)
   (write hmac) (newline)
   (string=? hmac "80070713463e7749b90c2dc24911e275")))
