TreeCursor
----------

.. autoclass:: tree_sitter.TreeCursor

   Methods
   -------

   .. automethod:: copy
   .. automethod:: goto_descendant
   .. automethod:: goto_first_child
   .. automethod:: goto_first_child_for_byte

      .. versionchanged:: 0.23.0

         Returns the child index instead of a `bool`.
   .. automethod:: goto_first_child_for_point

      .. versionchanged:: 0.23.0

         Returns the child index instead of a `bool`.
   .. automethod:: goto_last_child
   .. automethod:: goto_next_sibling
   .. automethod:: goto_parent
   .. automethod:: goto_previous_sibling
   .. automethod:: reset
   .. automethod:: reset_to

   Special Methods
   ---------------

   .. automethod:: __copy__

   Attributes
   ----------

   .. autoattribute:: depth
   .. autoattribute:: descendant_index
   .. autoattribute:: field_id
   .. autoattribute:: field_name
   .. autoattribute:: node
