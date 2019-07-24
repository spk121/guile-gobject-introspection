(use-modules (gi) (gi util)
             (test automake-test-lib)
             (srfi srfi-1)
             (system foreign))

(typelib-require (("Marshall" "1.0")
                  #:renamer (protect* '(sizeof short int long size_t))))

(automake-test
 (let ((x (list (1- (expt 2 7))
                (1- (expt 2 15))
                (1- (expt 2 31))
                (1- (expt 2 63))
                (1- (expt 2 (1- (* 8 (sizeof short)))))
                (1- (expt 2 (1- (* 8 (sizeof int)))))
                (1- (expt 2 (1- (* 8 (sizeof long)))))))
       (expected (list (- (expt 2 7))
                       (- (expt 2 15))
                       (- (expt 2 31))
                       (- (expt 2 63))
                       (- (expt 2 (1- (* 8 (sizeof short)))))
                       (- (expt 2 (1- (* 8 (sizeof int)))))
                       (- (expt 2 (1- (* 8 (sizeof long)))))))
       (procs (list int8-inout-max-min
                    int16-inout-max-min
                    int32-inout-max-min
                    int64-inout-max-min
                    short-inout-max-min
                    int-inout-max-min
                    long-inout-max-min)))
   (format #t "Input Before: ~S~%" x)
   (let ((y (map (lambda (proc val)
                   (proc val))
                 procs x)))
     (format #t "Input After: ~S~%" x)
     (format #t "Output: ~S~%" y)
     (format #t "Expected output: ~S~%" expected)
     (list= = y expected))))
