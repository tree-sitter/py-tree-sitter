Query
=====

.. autoclass:: tree_sitter.Query

   .. seealso:: `Query Syntax`_

   .. _Query Syntax: https://tree-sitter.github.io/tree-sitter/using-parsers#query-syntax

   .. note::

      The following predicates are supported by default:

      * ``#eq?``, ``#not-eq?``, ``#any-eq?``, ``#any-not-eq?``
      * ``#match?``, ``#not-match?``, ``#any-match?``, ``#any-not-match?``
      * ``#any-of?``, ``#not-any-of?``
      * ``#is?``, ``#is-not?``
      * ``#set!``

   Methods
   -------

   .. automethod:: capture_name
   .. automethod:: capture_quantifier
   .. automethod:: disable_capture
   .. automethod:: disable_pattern
   .. automethod:: end_byte_for_pattern
   .. automethod:: is_pattern_guaranteed_at_step
   .. automethod:: is_pattern_non_local
   .. automethod:: is_pattern_rooted
   .. automethod:: pattern_assertions
   .. automethod:: pattern_settings
   .. automethod:: start_byte_for_pattern
   .. automethod:: string_value

   Attributes
   ----------

   .. autoattribute:: capture_count
   .. autoattribute:: pattern_count
   .. autoattribute:: string_count
