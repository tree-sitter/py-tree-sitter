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

   .. automethod:: captures

      .. important::

         Predicates cannot be used if the tree was parsed from a callback.
   .. automethod:: disable_capture
   .. automethod:: disable_pattern
   .. automethod:: end_byte_for_pattern
   .. automethod:: is_pattern_guaranteed_at_step
   .. automethod:: is_pattern_non_local
   .. automethod:: is_pattern_rooted
   .. automethod:: matches

      .. important::

         Predicates cannot be used if the tree was parsed from a callback.
   .. automethod:: pattern_assertions
   .. automethod:: pattern_settings
   .. automethod:: set_byte_range
   .. automethod:: set_point_range
   .. automethod:: start_byte_for_pattern
   .. automethod:: set_match_limit
   .. automethod:: set_max_start_depth
   .. automethod:: set_timeout_micros

   Attributes
   ----------

   .. autoattribute:: capture_count
   .. autoattribute:: did_exceed_match_limit
   .. autoattribute:: match_limit
   .. autoattribute:: pattern_count
   .. autoattribute:: timeout_micros
