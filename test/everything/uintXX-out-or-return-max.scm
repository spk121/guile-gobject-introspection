(use-modules (gi) (gi util)
             (test automake-test-lib)
             (srfi srfi-1)
             (system foreign))

(typelib-require (("Marshall" "1.0")
                  #:renamer (protect* '(sizeof short int long size_t))))

(automake-test
 (let ((returned (list (uint8-return)
                       (uint16-return)
                       (uint32-return)
                       (uint64-return)
                       (ushort-return)
                       (uint-return)
                       (ulong-return)
                       (size-return)))
       (output (list (uint8-out)
                     (uint16-out)
                     (uint32-out)
                     (uint64-out)
                     (ushort-out)
                     (uint-out)
                     (ulong-out)
                     (size-out)))
       (expected (list (1- (expt 2 8))
                       (1- (expt 2 16))
                       (1- (expt 2 32))
                       (1- (expt 2 64))
                       (1- (expt 2 (* 8 (sizeof short))))
                       (1- (expt 2 (* 8 (sizeof int))))
                       (1- (expt 2 (* 8 (sizeof long))))
                       (1- (expt 2 (* 8 (sizeof size_t)))))))
   (format #t "Returned: ~S~%" returned)
   (format #t "Output  : ~S~%" output)
   (format #t "Expected: ~S~%" expected)
   (list= = returned output expected)))