(use-modules (gi) (gi repository) (gi types)
             (test automake-test-lib)
             (srfi srfi-43))

(unless (false-if-exception
         (begin (require "GLib" "2.0")
                (require "Gio" "2.0")))
  (exit EXIT_SKIPPED))

(for-each load-by-name
          (append (make-list 3 "GLib") (make-list 3 "Gio"))
          '("Variant" "VariantDict" "OptionArg"
            "Application" "ApplicationCommandLine" "ApplicationFlags"))

(automake-test
 (begin
   (let ((app (make <GApplication>
                #:application-id "gi.guile.Example"
                #:flags '(handles-command-line)))
         (success #f))
     ;; (display (flags->number (flags app)))
     (add-main-option app "hello" (char->integer #\h) 0
                      (symbol->option-arg 'string-array) "" #f)

     (connect app command-line
              (lambda (app command-line)
                (let* ((dict (get-options-dict command-line))
                       (hello (lookup-value dict "hello" #f))
                       (args (variant:get-strv hello)))
                  (vector-for-each (lambda (i arg) (format #t "Hello, ~a~%" arg))
                                   args)
                  (set! success (vector= string=? args #("world" "darkness, my old friend"))))
                (quit app)
                0))
     (application:run app #("command-line" "-h" "world" "-h" "darkness, my old friend"))
     success)))
