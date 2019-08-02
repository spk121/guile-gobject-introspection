;; Copyright (C), 2019 Michael L. Gran

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

(define-module (gi oop)
  #:use-module ((gi core-generics) #:select (connect))
  #:use-module (oop goops)
  #:use-module (srfi srfi-26)
  #:use-module (system foreign)
  #:export (<signal>
            make-signal
            connect-after)
  #:re-export (connect))

(eval-when (expand load eval)
  (load-extension "libguile-gi" "gig_init_object"))

(define-class <GFundamental> ()
  (value #:class <scm-slot>
         #:init-keyword #:value
         #:init-value %null-pointer))

(define-class <GEnum> (<GFundamental>)
  (obarray #:allocation #:each-subclass
           #:init-value '()))

(define-class <GFlags> (<GFundamental>)
  (obarray #:allocation #:each-subclass
           #:init-value '()))

(define-class <GBoxed> (<GFundamental>)
  (ref #:allocation #:each-subclass
       #:init-value (const %null-pointer))
  (unref #:allocation #:each-subclass
         #:init-value (const %null-pointer))
  (size #:allocation #:each-subclass
        #:init-value 0))

(define (%make-fundamental-class type dsupers ref unref)
  (make-class (cons <GFundamental> dsupers)
              `((ref #:allocation #:class
                     #:init-value ,ref)
                (unref #:allocation #:class
                       #:init-value ,unref))
              #:name type))

(define-class <signal> (<applicable-struct>)
  (name #:init-keyword #:name)
  (flags #:init-keyword #:flags
         #:init-value 0)
  (accumulator #:init-keyword #:accumulator
               #:init-value #f)
  (return-type #:init-keyword #:return-type
               #:init-value 0)
  (param-types #:init-keyword #:param-types
               #:init-value '()))

(define make-signal (cute make <signal> <...>))

(define-method (initialize (signal <signal>) initargs)
  (next-method)
  (slot-set! signal 'procedure (cut %emit <> signal <...>)))

(define (%find-signal signal type)
  (let* ((%signals (filter (compose (cute is-a? <> <signal>) method-procedure)
                           (generic-function-methods signal)))
         (cpl (class-precedence-list type))
         (signals (filter (compose (cute memq <> cpl) car method-specializers)
                          %signals)))
    (and (not (null? signals))
         (method-procedure
          (car (sort signals
                    (lambda (a b)
                      (let ((a-type (car (method-specializers a)))
                            (b-type (car (method-specializers b))))
                        (let lp ((cpl (class-precedence-list type)))
                          (let ((elt (car cpl)))
                            (cond
                             ((eq? a-type elt) #t)
                             ((eq? b-type elt) #f)
                             (else (lp (cdr cpl))))))))))))))

(define (%connect-generic obj signal detail handler after)
  (let ((real-signal (%find-signal signal (class-of obj))))
    (if real-signal (%connect obj real-signal detail handler after)
        (error "~S has no signal in ~S" obj signal))))

(define-method (connect obj (signal <generic>) (handler <procedure>))
  (%connect-generic obj signal #f handler #f))

(define-method (connect obj (signal <generic>) (detail <symbol>) (handler <procedure>))
  (%connect-generic obj signal detail handler #f))

(define-method (connect obj (signal <signal>) (handler <procedure>))
  (%connect obj signal #f handler))

(define-method (connect obj (signal <signal>) (detail <symbol>) (handler <procedure>))
  (%connect obj signal detail handler))

(define-method (connect-after obj (signal <generic>) (handler <procedure>))
  (%connect-generic obj signal #f handler #t))

(define-method (connect-after obj (signal <generic>) (detail <symbol>) (handler <procedure>))
  (%connect-generic obj signal detail handler #t))

(define-method (connect-after obj (signal <signal>) (handler <procedure>))
  (%connect obj signal #f handler #t))

(define-method (connect-after obj (signal <signal>) (detail <symbol>) (handler <procedure>))
  (%connect obj signal detail handler #t))
