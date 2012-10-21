.. _api:

API
===

.. module:: thegiant

This part of the documentation covers all the interfaces of The-Giant.  

Main Interface
--------------

Mainly you need to create an application (or a function) and give it to thegiant.server.run method

.. autofunction:: thegiant.server.listen
.. autofunction:: thegiant.server.run


Timers
~~~~~~~~~~

.. module:: thegiant.server

.. autofunction:: add_timer
.. autofunction:: stop_timer
.. autofunction:: restart_timer

Utilities
---------

These helper functions are used to format response values easily

.. module:: thegiant.helpers
.. autofunction:: reply

