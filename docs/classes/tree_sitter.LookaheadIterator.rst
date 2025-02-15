LookaheadIterator
=================

.. autoclass:: tree_sitter.LookaheadIterator
   :show-inheritance:

   Methods
   -------

   .. automethod:: iter_names
   .. automethod:: iter_symbols

      .. versionadded:: 0.25.0
   .. automethod:: reset

      .. versionadded:: 0.25.0
         Replaces the ``reset_state`` method

   Special Methods
   ---------------

   .. automethod:: __iter__

      .. versionchanged:: 0.25.0
         Iterates over ``tuple[int, str]``
   .. automethod:: __next__

      .. versionchanged:: 0.25.0
         Yields ``tuple[int, str]``

   Attributes
   ----------

   .. autoattribute:: current_symbol
   .. autoattribute:: current_symbol_name
   .. autoattribute:: language
