

class ASTVisitor:
    """
    Visitor to traverse a given abstract syntax tree

    The class implements the visitor design pattern
    to visit nodes inside an abstract syntax tree.
    The syntax tree is traversed in pre-order by employing
    a tree cursor. 

    Custom visitors should subclass the ASTVisitor:

    ```
    class AllNodesVisitor(tree_sitter.ASTVisitor):

        def visit_module(self, module_node):
            # Called for nodes of type module only
            ...
        
        def visit(self, node):
            # Called for all nodes where a specific handler does not exist
            # e.g. here called for all nodes that are not modules
            ...

        def visit_string(self, node):
            # Stops traversing subtrees rooted at a string
            # Traversal can be stopped by returning False
            return False

    ```
    
    """

    def visit(self, node):
        """Default handler that captures all nodes not already handled"""

    def visit_ERROR(self, error_node):
        """
        Handler for errors marked in AST.

        The subtree below an ERROR node can sometimes form unexpected AST structures.
        The default strategy is therefore to skip all subtrees rooted at an ERROR node.
        Override for a custom error handler.
        """
        return False

    # Traversing ----------------------------------------------------------------

    def on_visit(self, node):
        """
        Handles all nodes visted in AST and calls the underlying vistor methods.

        This method is called for all discovered AST nodes first.
        Override this to handle all nodes regardless of the defined visitor methods.

        Returning False stops the traversal of the subtree rooted at the given node.
        """
        visitor_fn = getattr(self, "visit_%s" % node.type, self.visit)
        return visitor_fn(node) is not False

    def walk(self, tree):
        """
        Walks a given (sub)tree by using the cursor API

        This method walks every node in a given subtree in pre-order traversal.
        During the traversal, the respective visitor method is called.
        """
        
        cursor   = tree.walk()
        has_next = True

        while has_next:
            current_node = cursor.node

            if self.on_visit(current_node):
                has_next = cursor.goto_first_child()
            else:
                has_next = False

            if not has_next: has_next = cursor.goto_next_sibling()

            previous_node = current_node

            while not has_next and cursor.goto_parent():
                has_next = cursor.goto_next_sibling()

                # Nodes marked as missing sometimes create loops in AST
                has_next = has_next and not _node_equal(previous_node, cursor.node)


# Helper --------------------------------

def _node_equal(n1, n2):
    if n1 == n2: return True
    try:
        return (n1.type == n2.type 
                    and n1.start_point == n2.start_point
                    and n1.end_point   == n2.end_point)
    except AttributeError:
        return n1 == n2
