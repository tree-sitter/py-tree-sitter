Parser
======

.. autoclass:: tree_sitter.Parser

   Methods
   -------

   .. automethod:: parse

      .. versionchanged:: 0.25.0
         * ``encoding`` can be one of ``"utf8", "utf16", "utf16le", "utf16be"``.
         * ``progress_callback`` parameter added.
   .. automethod:: print_dot_graphs
   .. automethod:: reset

   Attributes
   ----------

   .. autoattribute:: included_ranges
   .. autoattribute:: language
   .. autoattribute:: logger
   .. autoattribute:: timeout_micros

      .. deprecated:: 0.25.0
         Use the ``progress_callback`` in :meth:`parse`.
