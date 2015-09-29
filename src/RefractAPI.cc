//
//  RefractAPI.cc
//  drafter
//
//  Created by Jiri Kratochvil on 31/07/15.
//  Copyright (c) 2015 Apiary Inc. All rights reserved.
//

#include <iterator>

#include "SourceAnnotation.h"
#include "RefractDataStructure.h"
#include "RefractAPI.h"
#include "Render.h"

namespace drafter {

    // Forward Declarations
    refract::IElement* _ElementToRefract(const SectionInfo<snowcrash::Element>& element);

    namespace {

        typedef std::vector<refract::IElement*> RefractElements;
        typedef std::vector<const snowcrash::DataStructure*> DataStructures;

        void FindNamedTypes(const snowcrash::Elements& elements, DataStructures& found)
        {
            for (snowcrash::Elements::const_iterator i = elements.begin() ; i != elements.end() ; ++i) {

                if (i->element == snowcrash::Element::DataStructureElement) {
                    found.push_back(&(i->content.dataStructure));
                }
                else if (!i->content.resource.attributes.empty()) {
                    found.push_back(&i->content.resource.attributes);
                }
                else if (i->element == snowcrash::Element::CategoryElement) {
                    FindNamedTypes(i->content.elements(), found);
                }
            }
        }

        template <typename T>
        bool IsNull(const T* ptr)
        {
            return ptr == NULL;
        }

        void RemoveEmptyElements(RefractElements& elements)
        {
            elements.erase(std::remove_if(elements.begin(), elements.end(), IsNull<refract::IElement>), elements.end());
        }

        template <typename T, typename C, typename F>
        refract::IElement* CollectionToRefract(const C& collection, const F& transformFunctor, const std::string& key = std::string(), const refract::IElement::renderFlags renderType = refract::IElement::rCompact)
        {
            T* element = new T;
            RefractElements content;

            if (!key.empty()) {
                element->element(key);
            }

            std::transform(collection.begin(), collection.end(), std::back_inserter(content), transformFunctor);

            element->set(content);

            element->renderType(renderType);

            return element;
        }

        template<typename T, typename C, typename F>
        refract::IElement* CollectionToRefract(const SectionInfo<C>& collection, const F& transformFunctor, const std::string& key = std::string(), const refract::IElement::renderFlags renderType = refract::IElement::rCompact)
        {
            T* element = new T;
            RefractElements content;

            if (!key.empty()) {
                element->element(key);
            }

            SectionInfoCollection<C> sectionInfoCollection(collection.section, collection.sourceMap);

            std::transform(sectionInfoCollection.sections.begin(), sectionInfoCollection.sections.end(), std::back_inserter(content), transformFunctor);

            element->set(content);

            element->renderType(renderType);

            return element;
        }

    }

    refract::IElement* BytesRangeToRefract(const mdp::BytesRange& bytesRange)
    {
        refract::ArrayElement* range = new refract::ArrayElement;

        range->push_back(refract::IElement::Create(bytesRange.location));
        range->push_back(refract::IElement::Create(bytesRange.length));

        return range;
    }

    template<typename T>
    refract::IElement* SourceMapToRefract(const T& sourceMap) {
        refract::ArrayElement* element = new refract::ArrayElement;
        element->element("sourceMap");

        RefractElements ranges;
        std::transform(sourceMap.begin(), sourceMap.end(), std::back_inserter(ranges), BytesRangeToRefract);

        element->set(ranges);

        return element;
    }

    template <typename T>
    refract::ArrayElement* CreateArrayElement(const T& content, bool rFull)
    {
        refract::ArrayElement* array = new refract::ArrayElement;
        refract::IElement* value = refract::IElement::Create(content);

        if (rFull) {
            value->renderType(refract::IElement::rFull);
        }

        array->push_back(value);
        return array;
    }

    void RegisterNamedTypes(const snowcrash::Elements& elements)
    {
        DataStructures found;
        FindNamedTypes(elements, found);

        for (DataStructures::const_iterator i = found.begin(); i != found.end(); ++i) {

            if (!(*i)->name.symbol.literal.empty()) {
                refract::IElement* element = MSONToRefract(*(*i));
                GetNamedTypesRegistry().add(element);
            }
        }
    }

    refract::IElement* DataStructureToRefract(const snowcrash::DataStructure& dataStructure, bool expand)
    {
       refract::IElement* msonElement = MSONToRefract(dataStructure);

       if (expand) {
           refract::IElement* msonExpanded = ExpandRefract(msonElement, GetNamedTypesRegistry());
           msonElement = msonExpanded;
       }

       if (!msonElement) {
           return NULL;
       }

       refract::ObjectElement* element = new refract::ObjectElement;
       element->element(SerializeKey::DataStructure);
       element->push_back(msonElement);

       return element;
    }

    refract::IElement* _DataStructureToRefract(const SectionInfo<snowcrash::DataStructure>& dataStructure, bool expand)
    {
        refract::IElement* msonElement = _MSONToRefract(dataStructure);

        if (expand) {
            refract::IElement* msonExpanded = ExpandRefract(msonElement, GetNamedTypesRegistry());
            msonElement = msonExpanded;
        }

        if (!msonElement) {
            return NULL;
        }

        refract::ObjectElement* element = new refract::ObjectElement;
        element->element(SerializeKey::DataStructure);
        element->push_back(msonElement);

        return element;
    }

    refract::IElement* _MetadataToRefract(const SectionInfo<snowcrash::Metadata>& metadata)
    {
        refract::MemberElement* element = new refract::MemberElement;

        refract::ArrayElement* classes = CreateArrayElement(SerializeKey::User);
        classes->renderType(refract::IElement::rCompact);

        element->meta[SerializeKey::Classes] = classes;
        element->set(refract::IElement::Create(metadata.section.first), refract::IElement::Create(metadata.section.second));
        element->renderType(refract::IElement::rFull);

        return element;
    }

    refract::IElement* _CopyToRefract(const SectionInfo<std::string>& copy)
    {
        if (copy.section.empty()) {
            return NULL;
        }

        refract::IElement* element = refract::IElement::Create(copy.section);
        element->element(SerializeKey::Copy);

        //element->attributes["sourceMap"] = SourceMapToRefract(copy.sourceMap->sourceMap);

        return element;
    }

    refract::IElement* CopyToRefract(const std::string& copy)
    {
        if (copy.empty()) {
            return NULL;
        }

        refract::IElement* element = refract::IElement::Create(copy);
        element->element(SerializeKey::Copy);

        return element;
    }

    template<typename T>
    refract::IElement* ParameterValuesToRefract(const snowcrash::Parameter& parameter)
    {
        refract::ArrayElement* element = new refract::ArrayElement;
        RefractElements content;

        element->element(SerializeKey::Enum);

        // Add sample value
        if (!parameter.exampleValue.empty()) {
            refract::ArrayElement* samples = new refract::ArrayElement;
            samples->push_back(CreateArrayElement(LiteralTo<T>(parameter.exampleValue), true));
            element->attributes[SerializeKey::Samples] = samples;
        }

        // Add default value
        if (!parameter.defaultValue.empty()) {
            element->attributes[SerializeKey::Default] = CreateArrayElement(LiteralTo<T>(parameter.defaultValue), true);
        }

        for (snowcrash::Values::const_iterator it = parameter.values.begin();
             it != parameter.values.end();
             ++it) {

            content.push_back(refract::IElement::Create(LiteralTo<T>(*it)));
        }

        element->set(content);

        return element;
    }

    template<typename T>
    refract::IElement* ExtractParameter(const snowcrash::Parameter& parameter)
    {
        refract::IElement* element = NULL;

        if (parameter.values.empty()) {
            element = refract::IElement::Create(LiteralTo<T>(parameter.exampleValue));

            if (!parameter.defaultValue.empty()) {
                element->attributes[SerializeKey::Default] = refract::IElement::Create(LiteralTo<T>(parameter.defaultValue));
            }
        }
        else {
            element = ParameterValuesToRefract<T>(parameter);
        }

        return element;
    }

    refract::IElement* _ParameterToRefract(const SectionInfo<snowcrash::Parameter>& parameter)
    {
        refract::MemberElement* element = new refract::MemberElement;
        refract::IElement *value = NULL;

        // Parameter type, exampleValue, defaultValue, values
        if (parameter.section.type == "boolean") {
            value = ExtractParameter<bool>(parameter.section);
        }
        else if (parameter.section.type == "number") {
            value = ExtractParameter<double>(parameter.section);
        }
        else {
            value = ExtractParameter<std::string>(parameter.section);
        }

        element->set(refract::IElement::Create(parameter.section.name), value);

        // Description
        if (!parameter.section.description.empty()) {
            element->meta[SerializeKey::Description] = refract::IElement::Create(parameter.section.description);
        }

        // Parameter use
        if (parameter.section.use == snowcrash::RequiredParameterUse || parameter.section.use == snowcrash::OptionalParameterUse) {
            refract::ArrayElement* typeAttributes = new refract::ArrayElement;

            typeAttributes->push_back(refract::IElement::Create(parameter.section.use == snowcrash::RequiredParameterUse ? SerializeKey::Required : SerializeKey::Optional));
            element->attributes[SerializeKey::TypeAttributes] = typeAttributes;
        }

        return element;
    }

    refract::IElement* _ParametersToRefract(const SectionInfo<snowcrash::Parameters>& parameters)
    {
        return CollectionToRefract<refract::ObjectElement>(parameters, _ParameterToRefract, SerializeKey::HrefVariables, refract::IElement::rFull);
    }

    refract::IElement* _HeaderToRefract(const SectionInfo<snowcrash::Header>& header)
    {
        refract::MemberElement* element = new refract::MemberElement;
        element->set(refract::IElement::Create(header.section.first), refract::IElement::Create(header.section.second));

        return element;
    }

    refract::IElement* AssetToRefract(const snowcrash::Asset& asset, const std::string& contentType, bool messageBody = true)
    {
        if (asset.empty()) {
            return NULL;
        }

        refract::IElement* element = refract::IElement::Create(asset);

        element->element(SerializeKey::Asset);
        element->meta[SerializeKey::Classes] = refract::ArrayElement::Create(messageBody ? SerializeKey::MessageBody : SerializeKey::MessageSchema);

        if (!contentType.empty()) {
            element->attributes[SerializeKey::ContentType] = refract::IElement::Create(contentType);
        }

        return element;
    }

    refract::IElement* _PayloadToRefract(const SectionInfo<snowcrash::Payload>& payload, const SectionInfo<snowcrash::Action>& action)
    {
        refract::ArrayElement* element = new refract::ArrayElement;
        RefractElements content;

        // Use HTTP method to recognize if request or response
        if (action.isNull() || action.section.method.empty()) {
            element->element(SerializeKey::HTTPResponse);

            if (!payload.isNull()) {
                element->attributes[SerializeKey::StatusCode] = refract::IElement::Create(payload.section.name);
            }
        }
        else {
            element->element(SerializeKey::HTTPRequest);
            element->attributes[SerializeKey::Method] = refract::IElement::Create(action.section.method);

            if (!payload.isNull()) {
                element->attributes[SerializeKey::Title] = refract::IElement::Create(payload.section.name);
            }
        }

        // If no payload, return immediately
        if (payload.isNull()) {
            element->set(content);
            return element;
        }

        if (!payload.section.headers.empty()) {
            element->attributes[SerializeKey::Headers] = CollectionToRefract<refract::ArrayElement>(MAKE_SECTION_INFO(payload, headers),
                                                                                                    _HeaderToRefract,
                                                                                                    SerializeKey::HTTPHeaders,
                                                                                                    refract::IElement::rFull);
        }

        // Render using boutique
        snowcrash::Asset payloadBody = renderPayloadBody(payload.section, action.isNull() ? NULL : &action.section, GetNamedTypesRegistry());
        snowcrash::Asset payloadSchema = renderPayloadSchema(payload.section);

        content.push_back(_CopyToRefract(MAKE_SECTION_INFO(payload, description)));
        content.push_back(_DataStructureToRefract(MAKE_SECTION_INFO(payload, attributes)));

        // Get content type
        std::string contentType = getContentTypeFromHeaders(payload.section.headers);

        // Assets
        content.push_back(AssetToRefract(payloadBody, contentType));
        content.push_back(AssetToRefract(payloadSchema, contentType, false));

        RemoveEmptyElements(content);
        element->set(content);

        return element;
    }

    refract::IElement* _TransactionToRefract(const SectionInfo<snowcrash::TransactionExample>& transaction,
                                            const SectionInfo<snowcrash::Action>& action,
                                            const SectionInfo<snowcrash::Request>& request,
                                            const SectionInfo<snowcrash::Response>& response)
    {
        refract::ArrayElement* element = new refract::ArrayElement;
        RefractElements content;

        element->element(SerializeKey::HTTPTransaction);
        content.push_back(_CopyToRefract(MAKE_SECTION_INFO(transaction, description)));

        content.push_back(_PayloadToRefract(request, action));
        content.push_back(_PayloadToRefract(response, SectionInfo<snowcrash::Action>()));

        RemoveEmptyElements(content);
        element->set(content);

        return element;
    }

    refract::IElement* _ActionToRefract(const SectionInfo<snowcrash::Action>& action)
    {
        refract::ArrayElement* element = new refract::ArrayElement;
        RefractElements content;

        element->element(SerializeKey::Transition);
        element->meta[SerializeKey::Title] = refract::IElement::Create(action.section.name);

        if (!action.section.relation.str.empty()) {
            element->attributes[SerializeKey::Relation] = refract::IElement::Create(action.section.relation.str);
        }

        if (!action.section.uriTemplate.empty()) {
            element->attributes[SerializeKey::Href] = refract::IElement::Create(action.section.uriTemplate);
        }

        if (!action.section.parameters.empty()) {
            element->attributes[SerializeKey::HrefVariables] = _ParametersToRefract(MAKE_SECTION_INFO(action, parameters));
        }

        if (!action.section.attributes.empty()) {
            refract::IElement* dataStructure = _DataStructureToRefract(MAKE_SECTION_INFO(action, attributes));
            dataStructure->renderType(refract::IElement::rFull);
            element->attributes[SerializeKey::Data] = dataStructure;
        }

        content.push_back(_CopyToRefract(MAKE_SECTION_INFO(action, description)));

        typedef SectionInfoCollection<snowcrash::TransactionExamples> ExamplesType;
        ExamplesType examples(action.section.examples, action.sourceMap.examples);

        for (ExamplesType::ConstIterarator it = examples.sections.begin();
             it != examples.sections.end();
             ++it) {

            // When there are only responses
            if (it->section.requests.empty() && !it->section.responses.empty()) {

                typedef SectionInfoCollection<snowcrash::Responses> ResponsesType;
                ResponsesType responses(it->section.responses, it->sourceMap.responses);

                for (ResponsesType::ConstIterarator resIt = responses.sections.begin() ;
                     resIt != responses.sections.end();
                     ++resIt) {

                    content.push_back(_TransactionToRefract(*it, action, SectionInfo<snowcrash::Request>(), *resIt));
                }
            }

            // When there are only requests or both responses and requests
            typedef SectionInfoCollection<snowcrash::Requests> RequestsType;
            RequestsType requests(it->section.requests, it->sourceMap.requests);

            for (RequestsType::ConstIterarator reqIt = requests.sections.begin();
                 reqIt != requests.sections.end();
                 ++reqIt) {

                if (it->section.responses.empty()) {
                    content.push_back(_TransactionToRefract(*it, action, *reqIt, SectionInfo<snowcrash::Response>()));
                }

                typedef SectionInfoCollection<snowcrash::Responses> ResponsesType;
                ResponsesType responses(it->section.responses, it->sourceMap.responses);

                for (ResponsesType::ConstIterarator resIt = responses.sections.begin();
                     resIt != responses.sections.end();
                     ++resIt) {

                    content.push_back(_TransactionToRefract(*it, action, *reqIt, *resIt));
                }
            }
        }

        RemoveEmptyElements(content);
        element->set(content);

        return element;
    }

    refract::IElement* _ResourceToRefract(const SectionInfo<snowcrash::Resource>& resource)
    {
        refract::ArrayElement* element = new refract::ArrayElement;
        RefractElements content;

        element->element(SerializeKey::Resource);
        element->meta[SerializeKey::Title] = refract::IElement::Create(resource.section.name);
        element->attributes[SerializeKey::Href] = refract::IElement::Create(resource.section.uriTemplate);

        if (!resource.section.parameters.empty()) {
            element->attributes[SerializeKey::HrefVariables] = _ParametersToRefract(MAKE_SECTION_INFO(resource, parameters));
        }

        content.push_back(_CopyToRefract(MAKE_SECTION_INFO(resource, description)));
        content.push_back(_DataStructureToRefract(MAKE_SECTION_INFO(resource, attributes)));

        SectionInfoCollection<snowcrash::Actions> actions(resource.section.actions, resource.sourceMap.actions);
        std::transform(actions.sections.begin(), actions.sections.end(), std::back_inserter(content), _ActionToRefract);

        RemoveEmptyElements(content);
        element->set(content);

        return element;
    }

    refract::IElement* _CategoryToRefract(const SectionInfo<snowcrash::Element>& element)
    {
        refract::ArrayElement* category = new refract::ArrayElement;
        RefractElements content;

        category->element(SerializeKey::Category);

        if (element.section.category == snowcrash::Element::ResourceGroupCategory) {
            category->meta[SerializeKey::Classes] = CreateArrayElement(SerializeKey::ResourceGroup);
            category->meta[SerializeKey::Title] = refract::IElement::Create(element.section.attributes.name);
        }
        else if (element.section.category == snowcrash::Element::DataStructureGroupCategory) {
            category->meta[SerializeKey::Classes] = CreateArrayElement(SerializeKey::DataStructures);
        }

        if (!element.section.content.elements().empty()) {
            const snowcrash::SourceMap<snowcrash::Elements>& sourceMap = element.sourceMap.content.elements().collection.empty()
                ? snowcrash::SourceMap<snowcrash::Elements>()
                : element.sourceMap.content.elements();
            SectionInfoCollection<snowcrash::Elements> elements(element.section.content.elements(), sourceMap);
            std::transform(elements.sections.begin(), elements.sections.end(), std::back_inserter(content), _ElementToRefract);
        }

        RemoveEmptyElements(content);
        category->set(content);

        return category;
    }

    refract::IElement* _ElementToRefract(const SectionInfo<snowcrash::Element>& element) {
        switch (element.section.element) {
            case snowcrash::Element::ResourceElement:
                return _ResourceToRefract(MAKE_SECTION_INFO(element, content.resource));
            case snowcrash::Element::DataStructureElement:
                return _DataStructureToRefract(MAKE_SECTION_INFO(element, content.dataStructure));
            case snowcrash::Element::CopyElement:
                return _CopyToRefract(MAKE_SECTION_INFO(element, content.copy));
            case snowcrash::Element::CategoryElement:
                return _CategoryToRefract(element);
            default:
                throw snowcrash::Error("unknown type of api description element", snowcrash::ApplicationError);
        }
    }

    refract::IElement* BlueprintToRefract(const SectionInfo<snowcrash::Blueprint>& blueprint)
    {
        refract::ArrayElement* ast = new refract::ArrayElement;
        RefractElements content;

        ast->element(SerializeKey::Category);
        ast->meta[SerializeKey::Classes] = CreateArrayElement(SerializeKey::API);

        SectionInfo<std::string> name(blueprint.section.name, blueprint.sourceMap.name);
        refract::IElement* nameElement = refract::IElement::Create(blueprint.section.name);
        //nameElement->attributes["sourceMap"] = SourceMapToRefract(name.sourceMap->sourceMap);
        ast->meta[SerializeKey::Title] = nameElement;

        SectionInfo<std::string> description(blueprint.section.description, blueprint.sourceMap.description);
        content.push_back(_CopyToRefract(description));

        if (!blueprint.section.metadata.empty()) {
            SectionInfo<snowcrash::MetadataCollection> metadata(blueprint.section.metadata, blueprint.sourceMap.metadata);
            ast->attributes[SerializeKey::Meta] = CollectionToRefract<refract::ArrayElement>(metadata, _MetadataToRefract);
        }

        SectionInfoCollection<snowcrash::Elements> elements(blueprint.section.content.elements(), blueprint.sourceMap.content.elements());
        std::transform(elements.sections.begin(), elements.sections.end(), std::back_inserter(content), _ElementToRefract);

        RemoveEmptyElements(content);
        ast->set(content);

        return ast;
    }

} // namespace drafter
