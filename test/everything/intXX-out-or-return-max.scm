(use-modules (gi) (gi util)
             (test automake-test-lib)
             (srfi srfi-1)
             (system foreign))

(typelib-require (("Marshall" "1.0")
                  #:renamer (protect* '(sizeof short int long size_t))))

(automake-test
 (let ((returned (list (int8-return-max)
                       (int16-return-max)
                       (int32-return-max)
                       (int64-return-max)
                       (short-return-max)
                       (int-return-max)
                       (long-return-max)
                       (ssize-return-max)))
       (output (list (int8-out-max)
                     (int16-out-max)
                     (int32-out-max)
                     (int64-out-max)
                     (short-out-max)
                     (int-out-max)
                     (long-out-max)
                     (ssize-return-max)))
       (expected (list (1- (expt 2 7))
                       (1- (expt 2 15))
                       (1- (expt 2 31))
                       (1- (expt 2 63))
                       (1- (expt 2 (1- (* 8 (sizeof short)))))
                       (1- (expt 2 (1- (* 8 (sizeof int)))))
                       (1- (expt 2 (1- (* 8 (sizeof long)))))
                       (1- (expt 2 (1- (* 8 (sizeof size_t))))))))
   (format #t "Returned: ~S~%" returned)
   (format #t "Output  : ~S~%" output)
   (format #t "Expected: ~S~%" expected)
   (list= = returned output expected)))
