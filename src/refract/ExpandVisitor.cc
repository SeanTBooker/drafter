//
//  refract/ExpandVisitor.cc
//  librefract
//
//  Created by Jiri Kratochvil on 21/05/15.
//  Copyright (c) 2015 Apiary Inc. All rights reserved.
//

#include "Element.h"
#include "Visitors.h"
#include "Registry.h"

namespace refract
{

    namespace 
    {

        bool Expandable(const IElement& e) 
        {
            IsExpandableVisitor v;
            e.content(v);
            return v.get();
        }

        IElement* ExpandOrClone(const IElement* e, const Registry& registry) 
        {
            IElement* result = NULL;
            if(!e) {
                return result;
            }

            ExpandVisitor expander(registry);
            e->content(expander);
            result = expander.get();

            if(!result) {
                result = e->clone();
            }

            return result;
        }

        void CopyMetaId(IElement& dst, const IElement& src)
        {
            IElement::MemberElementCollection::const_iterator name = src.meta.find("id");
            if (name != src.meta.end() && (*name)->value.second && !(*name)->value.second->empty()) {
                dst.meta["id"] = (*name)->value.second->clone();
            }
        }

        IElement* FindMemberByKey(const ObjectElement& e, const std::string& name)
        {
            for (ObjectElement::ValueType::const_iterator it = e.value.begin()
                ; it != e.value.end()
                ; ++it ) {

                ComparableVisitor cmp(name, ComparableVisitor::key);
                (*it)->content(cmp);

                if(cmp.get()) { // key was recognized - it is save to cast to MemberElement
                    MemberElement* m = static_cast<MemberElement*>(*it);
                    return m->value.second;
                }
            }
            return NULL;
        }

        ObjectElement* FindNamedType(const Registry& registry, const std::string& name)
        {
            std::string en = name;
            ObjectElement* o = new ObjectElement;

            // walk recursive in registry an expand inheritance tree
            for (const IElement* parent = registry.find(en)
                ; parent && !isReserved(en)
                ; en = parent->element(), parent = registry.find(en) ) {

                // FIXME: while clone original element w/o meta - we lose `description`
                // must be fixed in spec
                IElement* clone = parent->clone(IElement::cAll | IElement::cNoMetaId);
                clone->meta["ref"] = IElement::Create(en);
                o->push_back(clone);
            }

            // FIXME: report if named type is not found
            // not implemented until error reporting
            //  if(o->value.empty()) { report error }

            return o;
        }

        void ExpandOrCloneMembers(std::vector<IElement*>& members, const ObjectElement& e, const Registry& registry, bool& hasRef)
        {
            for (ObjectElement::ValueType::const_iterator it = e.value.begin() ; it != e.value.end() ; ++it) {
                if((*it)->element() == "ref") {
                   hasRef = true; 
                }

                members.push_back(ExpandOrClone(*it, registry));
            }
        }

        IElement* ExpandNamedType(const ObjectElement& e, const Registry& registry) 
        {
            refract::ObjectElement* o = FindNamedType(registry, e.element());
            o->element("extend");

            CopyMetaId(*o, e);

            std::vector<IElement*> members;
            bool hasRef = false;
            ExpandOrCloneMembers(members, e, registry, hasRef);

            ObjectElement* origin = new ObjectElement; // wrapper for original object
            if(!members.empty()) {
                origin->set(members);
            }
            o->push_back(origin);

            return o;
        }

        IElement* ExpandReference(const ObjectElement& e, const Registry& registry)
        {
            refract::ObjectElement* o = new refract::ObjectElement;

            TypeQueryVisitor tq;
            StringElement* href = tq.as<StringElement>(FindMemberByKey(e, "href"));
            if(href) {
                return FindNamedType(registry, href->value);
            }
            else {
                // FIXME: report error
                // Ref Element does not contain "href" key, 
                // or value of `href` is not `StringElement`
                
                // can no be implemeted until error reporting system
                // for now - ignore silently
            }

            return o;
        }

        IElement* ExpandMembers(const ObjectElement& e, const Registry& registry)
        {
            std::vector<IElement*> members;
            bool hasRef = false;
            ExpandOrCloneMembers(members, e, registry, hasRef);
           
            refract::ObjectElement* o = static_cast<ObjectElement*>(e.clone(IElement::cAll ^ IElement::cValue));
            if(!members.empty()) {
                o->set(members);
            }

            if (hasRef) { // create addition object envelope over parent object
                ObjectElement* extend = new ObjectElement;
                extend->element("extend");
                extend->push_back(o);
                o = extend;
            }

            return o;
        }

    } // end of anonymous namespac

    ExpandVisitor::ExpandVisitor(const Registry& registry) : result(NULL), registry(registry) {};

    void ExpandVisitor::visit(const IElement& e) {

        if (!Expandable(e)) {
            return;
        }

        e.content(*this);
    }

    void ExpandVisitor::visit(const MemberElement& e) {

        if (!Expandable(e)) {
            return;
        }

        MemberElement* expanded = static_cast<MemberElement*>(e.clone(IElement::cAll ^ IElement::cValue));

        expanded->set(ExpandOrClone(e.value.first, registry), ExpandOrClone(e.value.second, registry));

        result = expanded;
    }


    void ExpandVisitor::visit(const ObjectElement& e) {

        if(!Expandable(e)) {  // do we have some expandable members?
            return;
        }

        std::string en = e.element();

        if(!isReserved(en)) { //A expand named type
            result = ExpandNamedType(e, registry);
        }
        else if (en == "ref") { // expand reference
            result = ExpandReference(e, registry);
        } 
        else { // walk throught members and expand them
            result = ExpandMembers(e, registry);
        } 

    }

    // do nothing, primitive elements are not expandable
    void ExpandVisitor::visit(const NullElement& e) {}
    void ExpandVisitor::visit(const StringElement& e) {}
    void ExpandVisitor::visit(const NumberElement& e) {}
    void ExpandVisitor::visit(const BooleanElement& e) {}
    
    // FIXME: can be ArrayElement Expandable?
    // probably if any of members will be Member|Object
    void ExpandVisitor::visit(const ArrayElement& e) {}

    IElement* ExpandVisitor::get() const {
        return result;
    }

}; // namespace refract
