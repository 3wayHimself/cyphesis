// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#ifndef SERVER_RESTORER_H
#define SERVER_RESTORER_H

class Entity;

// This class should never ever be instantiated, so the constructor is private
// and unimplemented. Instead the template should be instantiated with
// T as the class to be restored, and a reference to the object being
// restored is cast to a reference to this type, allowing code in
// this class to write to the protected methods.
// This class is probably going to want to directly decode data from the
// database layer.

template <class T>
class Restorer : public T {
  private:
    Restorer();
  public:
    void populate(int what_exactly);
    static Entity * restore(int what_exactly);
};

#endif // SERVER_RESTORER_H
