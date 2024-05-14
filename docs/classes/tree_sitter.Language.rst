Language
========

.. autoclass:: tree_sitter.Language


   Methods
   -------

   .. automethod:: field_id_for_name
   .. automethod:: field_name_for_id
   .. automethod:: id_for_node_kind
   .. automethod:: lookahead_iterator
   .. automethod:: next_state
   .. automethod:: node_kind_for_id
   .. automethod:: node_kind_is_named
   .. automethod:: node_kind_is_visible
   .. automethod:: query

   Special Methods
   ---------------

   .. automethod:: __eq__
   .. automethod:: __hash__

      .. important::

         On 32-bit platforms, you must use ``hash(self) & 0xFFFFFFFF`` to get the actual hash.
   .. automethod:: __ne__
   .. automethod:: __repr__


   Attributes
   ----------

   .. autoattribute:: field_count
   .. autoattribute:: node_kind_count
   .. autoattribute:: parse_state_count
   .. autoattribute:: version
