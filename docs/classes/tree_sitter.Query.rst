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

      .. versionchanged:: 0.23.0

         Range arguments removed, :class:`predicate <QueryPredicate>` argument added,
         return type changed to ``dict[str, list[Node]]``.
   .. automethod:: disable_capture

      .. versionadded:: 0.23.0
   .. automethod:: disable_pattern

      .. versionadded:: 0.23.0
   .. automethod:: end_byte_for_pattern

      .. versionadded:: 0.23.0
   .. automethod:: is_pattern_guaranteed_at_step

      .. versionadded:: 0.23.0
   .. automethod:: is_pattern_non_local

      .. versionadded:: 0.23.0
   .. automethod:: is_pattern_rooted

      .. versionadded:: 0.23.0
   .. automethod:: matches

      .. important::

         Predicates cannot be used if the tree was parsed from a callback.

      .. versionchanged:: 0.23.0

         Range arguments removed, :class:`predicate <QueryPredicate>` argument added,
         return type changed to ``list[tuple[int, dict[str, list[Node]]]]``.
   .. automethod:: pattern_assertions

      .. versionadded:: 0.23.0
   .. automethod:: pattern_settings

      .. versionadded:: 0.23.0
   .. automethod:: set_byte_range

      .. versionadded:: 0.23.0
   .. automethod:: set_point_range

      .. versionadded:: 0.23.0
   .. automethod:: start_byte_for_pattern

      .. versionadded:: 0.23.0
   .. automethod:: set_match_limit

      .. versionadded:: 0.23.0
   .. automethod:: set_max_start_depth

      .. versionadded:: 0.23.0

   Attributes
   ----------

   .. autoattribute:: capture_count

      .. versionadded:: 0.23.0
   .. autoattribute:: did_exceed_match_limit

      .. versionadded:: 0.23.0
   .. autoattribute:: match_limit

      .. versionadded:: 0.23.0
   .. autoattribute:: pattern_count

      .. versionadded:: 0.23.0
