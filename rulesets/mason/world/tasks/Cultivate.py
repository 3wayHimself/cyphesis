#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 2006 Al Riddoch (See the file COPYING for details).

from atlas import *
from physics import *
from physics import Quaternion
from physics import Vector3D

from random import *

import server

class Cultivate(server.Task):
    """ A proof of concept task germinating seeds into plants."""
    def sow_operation(self, op):
        """ Op handler for sow op which activates this task """
        # print "Cultivate.sow"

        if len(op) < 1:
            sys.stderr.write("Cultivate task has no target in sow op")

        # FIXME Use weak references, once we have them
        self.target = server.world.get_object_ref(op[0].id)
        self.tool = op.to

    def tick_operation(self, op):
        """ Op handler for regular tick op """
        # print "Cultivate.tick"
        res=Oplist()

        if self.target() is None:
            # print "Target is no more"
            self.irrelevant()
            return

        if square_distance(self.character.location, self.target().location) > \
           self.character.location.bbox.square_bounding_radius():
            self.progress = 0
            self.rate = 0
            return self.next_tick(1.75)

        if self.rate < 0.01:
            self.progress = 0
            self.rate = 1 / 1.75
            return self.next_tick(1.75)

        new_loc = self.target().location.copy()
        new_loc.orientation = self.target().location.orientation
        create=Operation("create", Entity(type = self.target().germinates,
                                          mass = self.target().mass,
                                          bbox = [-0.02, -0.02, 0,
                                                  0.02, 0.02, 0.12],
                                          location = new_loc), to = self.target())
        res.append(create)

        set=Operation("set", Entity(self.target().id, status=-1), to=self.target())
        res.append(set)

        self.progress = 1
        self.rate = 0
        
        res.append(self.next_tick(1.75))

        return res
