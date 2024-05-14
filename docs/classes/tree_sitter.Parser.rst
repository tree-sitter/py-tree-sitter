Parser
======

.. autoclass:: tree_sitter.Parser

   .. versionadded:: 0.22.0

      constructor

   Methods
   -------


   .. automethod:: parse

      .. versionchanged:: 0.22.0

         Now accepts an ``encoding`` parameter.
      .. versionchanged:: 0.23.0

         No longer accepts a ``keep_text`` parameter.
   .. automethod:: reset

   Attributes
   ----------

   .. autoattribute:: included_ranges

      .. versionadded:: 0.22.0
   .. autoattribute:: language

      .. versionadded:: 0.22.0
   .. autoattribute:: timeout_micros

      .. versionadded:: 0.22.0
