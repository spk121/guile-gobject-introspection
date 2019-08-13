/* Requires GNU indent 2.2.12
 * - use long options for readability
 * - group options logically
 * - indent apparently does not understand attributes, so surround function
 *   declarations using them *INDENT-OFF* and *INDENT-ON* comments.
 * - you may wish to indent in-place without creating backups using
 *   `VERSION_CONTROL=none indent ...'
 */
--line-length 99
--indent-level 4
--indent-label 2
--no-tabs

/* alignment */
--continue-at-parentheses
--break-after-boolean-operator

/* brace style: "basically" Stroustrup */
--braces-after-struct-decl-line
--braces-after-func-def-line
--braces-on-if-line
--dont-cuddle-else
--case-brace-indentation 0

/* spacing */
--no-space-after-function-call-names
--space-after-if
--no-space-after-casts

/* blank lines */
--no-blank-lines-after-commas
--blank-lines-after-procedures

/* don't confuse make
 * does this even work?
 */
--preserve-mtime

/* misc... */
--start-left-side-of-comments
--pointer-align-right

/* put typedefs here
 * GLib contains a lot of them, so really only update this list if something
 * breaks, e.g. misaligned pointers
 */
-T SCM

-T ffi_cif
-T ffi_closure

-T gboolean
-T gchar
-T gdouble
-T gfloat
-T gint
-T gint16
-T gint32
-T gint64
-T gint8
-T gpointer
-T gsize
-T guint
-T gssize
-T guint16
-T guint32
-T guint64
-T guint8
-T gunichar
-T GArray
-T GClosure
-T GDestroyNotify
-T GError
-T GLogLevelFlags
-T GLogField
-T GObject
-T GObjectClass
-T GParamSpec
-T GPtrArray
-T GQuark
-T GSignalQuery
-T GString
-T GTypeInstance
-T GValue

-T GIArgInfo
-T GIArgument
-T GIBaseInfo
-T GICallableInfo
-T GICallbackInfo
-T GIConstantInfo
-T GIEnumInfo
-T GIFunctionInfo
-T GIInterfaceInfo
-T GIObjectInfo
-T GIPropertyInfo
-T GISignalInfo
-T GITypeInfo

-T GigArgMapEntry
-T GigArgMap
-T GigBoxedFuncs
-T GigCallback
-T GigFunction
-T GigGsubr
-T GigRepositoryNested
-T GigSignalSpec
-T GigSignalSlot
-T GigTypeMeta
-T GigTypeRefFunction
-T GigTypeUnrefFunction
