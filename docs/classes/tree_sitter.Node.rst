Node
====

.. autoclass:: tree_sitter.Node

   Methods
   -------

   .. automethod:: child
   .. automethod:: child_by_field_id
   .. automethod:: child_by_field_name
   .. automethod:: children_by_field_id
   .. automethod:: children_by_field_name
   .. automethod:: descendant_for_byte_range
   .. automethod:: descendant_for_point_range
   .. automethod:: edit
   .. automethod:: field_name_for_child
   .. automethod:: named_child
   .. automethod:: named_descendant_for_byte_range
   .. automethod:: named_descendant_for_point_range
   .. automethod:: walk

   Special Methods
   ---------------

   .. automethod:: __eq__
   .. automethod:: __hash__
   .. automethod:: __ne__
   .. automethod:: __repr__
   .. automethod:: __str__

   Attributes
   ----------

   .. autoattribute:: byte_range
   .. autoattribute:: child_count
   .. autoattribute:: children
   .. autoattribute:: descendant_count
   .. autoattribute:: end_byte
   .. autoattribute:: end_point
   .. autoattribute:: grammar_id
   .. autoattribute:: grammar_name
   .. autoattribute:: has_changes
   .. autoattribute:: has_error
   .. autoattribute:: id
   .. autoattribute:: is_error
   .. autoattribute:: is_extra
   .. autoattribute:: is_missing
   .. autoattribute:: is_named
   .. autoattribute:: kind_id
   .. autoattribute:: named_child_count
   .. autoattribute:: named_children
   .. autoattribute:: next_named_sibling
   .. autoattribute:: next_parse_state
   .. autoattribute:: next_sibling
   .. autoattribute:: parent
   .. autoattribute:: parse_state
   .. autoattribute:: prev_named_sibling
   .. autoattribute:: prev_sibling
   .. autoattribute:: range
   .. autoattribute:: start_byte
   .. autoattribute:: start_point
   .. autoattribute:: text
   .. autoattribute:: type
