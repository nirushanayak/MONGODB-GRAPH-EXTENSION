/**
 *    Copyright (C) 2025-present MongoDB, Inc.
 */

 #include "mongo/db/pipeline/document_source_bidirectional_graph_lookup.h"
 #include "mongo/db/pipeline/expression_context.h"
 #include "mongo/db/pipeline/lite_parsed_pipeline.h"
 #include "mongo/db/pipeline/document_source_match.h"
 #include "mongo/util/namespace_string_util.h"
 #include "mongo/db/exec/document_value/document.h"
 #include "mongo/db/exec/document_value/value_comparator.h"
 #include "mongo/db/pipeline/document_path_support.h"
 #include "mongo/db/pipeline/pipeline.h"
 #include "mongo/db/views/resolved_view.h"
 #include "mongo/db/query/query_knobs_gen.h"
 #include "mongo/base/init.h"
 #include "mongo/logv2/log.h"
 #include "mongo/db/pipeline/expression.h"
 #include "mongo/db/pipeline/expression_dependencies.h"
 
 #define MONGO_LOGV2_DEFAULT_COMPONENT ::mongo::logv2::LogComponent::kQuery
 
 namespace mongo {
 
 // Register stage
 REGISTER_DOCUMENT_SOURCE(bidirectionalGraphLookup,
                          DocumentSourceBidirectionalGraphLookup::LiteParsed::parse,
                          DocumentSourceBidirectionalGraphLookup::createFromBson,
                          AllowedWithApiStrict::kAlways);
 
 ALLOCATE_DOCUMENT_SOURCE_ID(bidirectionalGraphLookup, DocumentSourceBidirectionalGraphLookup::id);
 
 namespace {
 NamespaceString parseGraphLookupFromAndResolveNamespace(const BSONElement& elem,
                                                         const DatabaseName& defaultDb) {
     uassert(ErrorCodes::FailedToParse,
             str::stream() << "$bidirectionalGraphLookup 'from' field must be a string, but found "
                           << typeName(elem.type()),
             elem.type() == String);
 
     NamespaceString fromNss = NamespaceStringUtil::deserialize(defaultDb, elem.valueStringData());
     uassert(ErrorCodes::InvalidNamespace,
             str::stream() << "invalid $bidirectionalGraphLookup namespace: " << fromNss.toStringForErrorMsg(),
             fromNss.isValid());
     return fromNss;
 }
 }  // namespace
 
 // LiteParsed implementation
 std::unique_ptr<DocumentSourceBidirectionalGraphLookup::LiteParsed>
 DocumentSourceBidirectionalGraphLookup::LiteParsed::parse(
     const NamespaceString& nss, const BSONElement& spec, const LiteParserOptions& options) {
     uassert(ErrorCodes::FailedToParse,
             "$bidirectionalGraphLookup must be an object",
             spec.type() == Object);
 
     auto specObj = spec.Obj();
     auto fromElement = specObj["from"];
     uassert(ErrorCodes::FailedToParse,
             "missing 'from' option to $bidirectionalGraphLookup stage",
             fromElement);
 
     NamespaceString fromNs = parseGraphLookupFromAndResolveNamespace(fromElement, nss.dbName());
     return std::make_unique<LiteParsed>(spec.fieldName(), std::move(fromNs));
 }
 
 PrivilegeVector DocumentSourceBidirectionalGraphLookup::LiteParsed::requiredPrivileges(
     bool isMongos, bool bypassDocumentValidation) const {
     return {Privilege(ResourcePattern::forExactNamespace(_foreignNss), ActionType::find)};
 }
 
 // Static factory
 boost::intrusive_ptr<DocumentSource> DocumentSourceBidirectionalGraphLookup::createFromBson(
     BSONElement elem, const boost::intrusive_ptr<ExpressionContext>& pExpCtx) {
     
     uassert(ErrorCodes::FailedToParse,
             "$bidirectionalGraphLookup must be an object",
             elem.type() == Object);
     
     auto spec = elem.Obj();
     VariablesParseState vps = pExpCtx->variablesParseState;
     
     NamespaceString from;
     std::string as;
     std::string connectFromField;
     std::string connectToField;
     boost::intrusive_ptr<Expression> startWith;
     boost::intrusive_ptr<Expression> target;
     boost::optional<BSONObj> restrictSearchWithMatch;
     boost::optional<std::string> depthField;
     boost::optional<long long> maxDepth;
     
     for (auto&& argument : spec) {
         const auto argName = argument.fieldNameStringData();
         
         if (argName == "from") {
             from = parseGraphLookupFromAndResolveNamespace(argument, pExpCtx->getNamespaceString().dbName());
         } else if (argName == "as") {
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "expected string for 'as' field",
                     argument.type() == String);
             as = argument.String();
         } else if (argName == "connectFromField") {
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "expected string for 'connectFromField' field",
                     argument.type() == String);
             connectFromField = argument.String();
         } else if (argName == "connectToField") {
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "expected string for 'connectToField' field",
                     argument.type() == String);
             connectToField = argument.String();
         } else if (argName == "startWith") {
             startWith = Expression::parseOperand(pExpCtx.get(), argument, vps);
         } else if (argName == "target") {
             target = Expression::parseOperand(pExpCtx.get(), argument, vps);
         } else if (argName == "restrictSearchWithMatch") {
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "expected object for 'restrictSearchWithMatch'",
                     argument.type() == Object);
             restrictSearchWithMatch = argument.embeddedObject().getOwned();
         } else if (argName == "depthField") {
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "expected string for 'depthField'",
                     argument.type() == String);
             depthField = argument.String();
         } else if (argName == "maxDepth") {
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "expected number for 'maxDepth'",
                     argument.isNumber());
             maxDepth = argument.safeNumberLong();
             uassert(ErrorCodes::FailedToParse,
                     str::stream() << "maxDepth must be non-negative",
                     *maxDepth >= 0);
         } else {
             uasserted(ErrorCodes::FailedToParse,
                       str::stream() << "unknown argument to $bidirectionalGraphLookup: " 
                                     << argument.fieldName());
         }
     }
     
     // Validate required fields
     uassert(ErrorCodes::FailedToParse,
             "$bidirectionalGraphLookup requires 'from', 'as', 'connectFromField', "
             "'connectToField', 'startWith', and 'target'",
             !from.isEmpty() && !as.empty() && !connectFromField.empty() && 
             !connectToField.empty() && startWith && target);
     
     return make_intrusive<DocumentSourceBidirectionalGraphLookup>(
         pExpCtx,
         std::move(from),
         std::move(as),
         std::move(connectFromField),
         std::move(connectToField),
         std::move(startWith),
         std::move(target),
         restrictSearchWithMatch,
         depthField,
         maxDepth);
 }
 
 // Constructor
 DocumentSourceBidirectionalGraphLookup::DocumentSourceBidirectionalGraphLookup(
     const boost::intrusive_ptr<ExpressionContext>& pExpCtx,
     NamespaceString fromNs,
     std::string asField,
     std::string connectFromField,
     std::string connectToField,
     boost::intrusive_ptr<Expression> startWith,
     boost::intrusive_ptr<Expression> target,
     boost::optional<BSONObj> restrictSearchWithMatch,
     boost::optional<std::string> depthField,
     boost::optional<long long> maxDepth)
     : DocumentSource(kStageName, pExpCtx),
       _fromNs(std::move(fromNs)),
       _as(std::move(asField)),
       _connectFromField(std::move(connectFromField)),
       _connectToField(std::move(connectToField)),
       _startWith(std::move(startWith)),
       _target(std::move(target)),
       _additionalFilter(restrictSearchWithMatch),
       _depthField(depthField),
       _maxDepth(maxDepth),
       _maxMemoryUsageBytes(internalDocumentSourceGraphLookupMaxMemoryBytes.load()),
       _forwardVisited(pExpCtx->getValueComparator().makeUnorderedValueMap<SearchNode>()),
       _backwardVisited(pExpCtx->getValueComparator().makeUnorderedValueMap<SearchNode>()),
       _forwardFrontier(pExpCtx->getValueComparator().makeFlatUnorderedValueSet()),
       _backwardFrontier(pExpCtx->getValueComparator().makeFlatUnorderedValueSet()) {
     
     const auto& resolvedNamespace = pExpCtx->getResolvedNamespace(_fromNs);
     _fromExpCtx = pExpCtx->copyForSubPipeline(resolvedNamespace.ns, resolvedNamespace.uuid);
     _fromExpCtx->setInLookup(true);
 }
 
 // DocumentSource interface
 const char* DocumentSourceBidirectionalGraphLookup::getSourceName() const {
     return kStageName.rawData();
 }
 
 DocumentSourceBidirectionalGraphLookup::Id DocumentSourceBidirectionalGraphLookup::getId() const {
     return id;
 }
 
 DocumentSource::GetNextResult DocumentSourceBidirectionalGraphLookup::doGetNext() {
     if (_executed)
         return GetNextResult::makeEOF();
 
     auto input = pSource->getNext();
     LOGV2(9999999, "doGetNext() - input received: {state}", "state"_attr = input.getStatus());

     if (!input.isAdvanced())
         return input;
 
     _inputDoc = input.releaseDocument();
     _executed = true;
 
     performBidirectionalSearch();
 
     MutableDocument output(_inputDoc);
     output.setField(_as, Value(_results));
     return output.freeze();
 }
 
 void DocumentSourceBidirectionalGraphLookup::performBidirectionalSearch() {
     // Clear previous search state
     resetSearchState();
     
     // Initialize forward search with startWith value
     auto startVal = _startWith->evaluate(_inputDoc, &pExpCtx->variables);
     LOGV2(9999999, "startWith evaluated to: {val}", "val"_attr = startVal);
     if (startVal.isArray()) {
         for (const auto& val : startVal.getArray()) {
             _forwardFrontier.insert(val);
         }
     } else {
         _forwardFrontier.insert(startVal);
         LOGV2(9999999, "Inserted into forward frontier: {val}", "val"_attr = startVal);

     }
     
     // Initialize backward search with target value
     auto targetVal = _target->evaluate(_inputDoc, &pExpCtx->variables);
     LOGV2(9999999, "target evaluated to: {val}", "val"_attr = targetVal);
     if (targetVal.isArray()) {
         for (const auto& val : targetVal.getArray()) {
             _backwardFrontier.insert(val);
         }
     } else {
         _backwardFrontier.insert(targetVal);
     }
     
     size_t depth = 0;
     boost::optional<BidirectionalPath> pathFound;
     
     while (!_forwardFrontier.empty() && !_backwardFrontier.empty() && 
            (!_maxDepth || depth <= *_maxDepth)) {
         
         // Check if there's a meeting point
         boost::optional<Value> pathFound = checkMeetingPoint();
         if (pathFound) {
             _results = reconstructPath(*pathFound);
             return;
         }
         
         // Expand both frontiers
         bool forwardExpanded = expandFrontier(true);
         bool backwardExpanded = expandFrontier(false);
         
         if (!forwardExpanded && !backwardExpanded) {
             break;  // No more nodes to explore
         }
         
         ++depth;
         checkMemoryUsage();
     }
     
     // No path found
     _results.clear();
 }
 
//  bool DocumentSourceBidirectionalGraphLookup::expandFrontier(bool isForward) {
//     auto& frontier = isForward ? _forwardFrontier : _backwardFrontier;
//     auto& visited = isForward ? _forwardVisited : _backwardVisited;
//     const auto& connectField = isForward ? _connectFromField : _connectToField;
//     const auto& matchField = isForward ? _connectToField : _connectFromField;


//     if (frontier.empty()) {
//         return false;
//     }

//     // Snapshot the current frontier before clearing it
//     ValueFlatUnorderedSet frontierSnapshot = frontier;
//     frontier.clear();
//     LOGV2(9999999, "Starting expandFrontier. Initial snapshot size: {s}", "s"_attr = frontierSnapshot.size());

//     _frontierUsageBytes = 0;

//     bool expanded = false;

//     for (const auto& valueBeingMatched : frontierSnapshot) {
//         // Build a separate $match stage for each frontier node
        
//         BSONObjBuilder match;
//         BSONObjBuilder outer(match.subobjStart("$match"));
//         BSONObjBuilder inner(outer.subobjStart(matchField));
//         BSONArrayBuilder in(inner.subarrayStart("$in"));
//         in << valueBeingMatched;
//         in.done();
//         inner.done();
//         outer.done();
//         LOGV2(9999999, "Generated match: {match}", "match"_attr = match.obj().toString());

//         auto pipeline = buildPipeline(match.obj());
//         LOGV2(9999999, "Expanding valueBeingMatched: {val}, matchField: {mf}", 
//             "val"_attr = valueBeingMatched.toString(), 
//             "mf"_attr = matchField);
      

//         while (auto next = pipeline->getNext()) {
//             if (!next) {
//                 LOGV2(9999999, "Got null document from pipeline — skipping.");
//                 continue;
//             }
//             LOGV2(9999999, "Fetched doc: {doc}", "doc"_attr = next->toString());
//             auto id = next->getField("_id");

//             if (visited.find(id) == visited.end()) {
//                 SearchNode node;
//                 node.id = id;
//                 node.depth = visited.size();
//                 node.isForwardDirection = isForward;
//                 node.parentId = valueBeingMatched;  // ✅ this now works
//                 visited[id] = node;
//                 _visitedUsageBytes += id.getApproximateSize() + next->getApproximateSize();

//                 // Add new frontier values
//                 document_path_support::visitAllValuesAtPath(
//                     *next,
//                     FieldPath(connectField),
//                     [&](const Value& connectVal) {
//                         if (visited.find(connectVal) == visited.end()) {
//                             frontier.insert(connectVal);
//                             _frontierUsageBytes += connectVal.getApproximateSize();
//                         }
//                     });

//                 expanded = true;
//             }
//         }
//     }
//     LOGV2(9999999, "{dir} expansion: visited={v}, frontier={f}",
//         "dir"_attr = isForward ? "Forward" : "Backward",
//         "v"_attr = visited.size(),
//         "f"_attr = frontier.size());
  

//     return expanded;
// }
bool DocumentSourceBidirectionalGraphLookup::expandFrontier(bool isForward) {
    auto& frontier = isForward ? _forwardFrontier : _backwardFrontier;
    auto& visited = isForward ? _forwardVisited : _backwardVisited;
    const auto& connectField = isForward ? _connectFromField : _connectToField;
    const auto& matchField = isForward ? _connectToField : _connectFromField;

    if (frontier.empty()) {
        return false;
    }

    ValueFlatUnorderedSet frontierSnapshot = frontier;
    frontier.clear();
    LOGV2(9999999, "Starting expandFrontier. Initial snapshot size: {s}", "s"_attr = frontierSnapshot.size());

    _frontierUsageBytes = 0;
    bool expanded = false;

    for (const auto& valueBeingMatched : frontierSnapshot) {
        uassert(6969001, "matchField must not be empty", !matchField.empty());

        BSONObj matchObj;
        try {
            BSONObjBuilder matchBuilder;
            {
                BSONObjBuilder outer(matchBuilder.subobjStart("$match"));
                outer.append(matchField, BSON("$in" << BSON_ARRAY(valueBeingMatched)));
            }
            matchObj = matchBuilder.obj();
        } catch (const DBException& ex) {
            LOGV2_WARNING(9999999, "Failed to build match object: {error}", "error"_attr = ex.toString());
            continue;
        }

        LOGV2(9999999, "Generated match: {match}", "match"_attr = matchObj.toString());

        auto pipeline = buildPipeline(matchObj);
        uassert(6969002, "Failed to build pipeline", pipeline != nullptr);

        LOGV2(9999999,
              "Expanding valueBeingMatched: {val}, matchField: {mf}",
              "val"_attr = valueBeingMatched.toString(),
              "mf"_attr = matchField);

        while (auto next = pipeline->getNext()) {
            LOGV2(9999999, "Fetched doc: {doc}", "doc"_attr = next->toString());

            auto id = next->getField("_id");
            if (visited.find(id) == visited.end()) {
                SearchNode node;
                node.id = id;
                node.depth = visited.size();
                node.isForwardDirection = isForward;
                node.parentId = valueBeingMatched;
                visited[id] = node;
                _visitedUsageBytes += id.getApproximateSize() + next->getApproximateSize();

                document_path_support::visitAllValuesAtPath(
                    *next,
                    FieldPath(connectField),
                    [&](const Value& connectVal) {
                        if (visited.find(connectVal) == visited.end()) {
                            frontier.insert(connectVal);
                            _frontierUsageBytes += connectVal.getApproximateSize();
                        }
                    });

                expanded = true;
            }
        }
    }

    LOGV2(9999999,
          "{dir} expansion complete: visited={v}, frontier={f}",
          "dir"_attr = isForward ? "Forward" : "Backward",
          "v"_attr = visited.size(),
          "f"_attr = frontier.size());

    return expanded;
}


//  boost::optional<DocumentSourceBidirectionalGraphLookup::BidirectionalPath> 
//  DocumentSourceBidirectionalGraphLookup::checkMeetingPoint() {
//      for (const auto& [id, forwardNode] : _forwardVisited) {
//          if (_backwardVisited.find(id) != _backwardVisited.end()) {
//              BidirectionalPath path;
//              path.meetingNode = Document{{"_id", id}};
//              return path;
//          }
//      }
//      return boost::none;
//  }

boost::optional<Value> DocumentSourceBidirectionalGraphLookup::checkMeetingPoint() {
    LOGV2(9999999, "Checking meeting point - forward size: {fwd}, backward size: {bwd}",
        "fwd"_attr = _forwardVisited.size(),
        "bwd"_attr = _backwardVisited.size());

    for (const auto& [id, _] : _forwardVisited) {
        if (_backwardVisited.find(id) != _backwardVisited.end()) {
            LOGV2(9999999, "Meeting point found at: {id}", "id"_attr = id.toString());
            return id;
        }
    }
    return boost::none;
}


 
//  std::vector<Document> DocumentSourceBidirectionalGraphLookup::reconstructPath(
//      const BidirectionalPath& bidirectionalPath) {
//      // Return only the nodes in the actual path, not all visited nodes
//      std::vector<Document> results;
     
//      // If we found a meeting point, reconstruct the path
//      if (!bidirectionalPath.meetingNode.empty()) {
//          // In a proper implementation, we'd trace back through parent pointers
//          // For now, just add the meeting node to show the algorithm is working
//          MutableDocument doc;
//          doc.addField("_id", bidirectionalPath.meetingNode["_id"]);
//          doc.addField("type", Value("meeting_point"_sd));
//          results.push_back(doc.freeze());
//      }
     
//      return results;
//  }
 
std::vector<Document> DocumentSourceBidirectionalGraphLookup::reconstructPath(Value meetingId) {
    LOGV2(9999999, "Reconstructing path from meetingId: {id}", "id"_attr = meetingId.toString());

    std::vector<Document> result;

    std::vector<Value> forwardPath;
    for (auto id = meetingId; !_forwardVisited[id].parentId.missing(); id = _forwardVisited[id].parentId) {
        LOGV2(9999999, "Forward path step: {id}", "id"_attr = id.toString());
        forwardPath.push_back(id);
    }
    std::reverse(forwardPath.begin(), forwardPath.end());

    std::vector<Value> backwardPath;
    for (auto id = meetingId; !_backwardVisited[id].parentId.missing(); id = _backwardVisited[id].parentId) {
        backwardPath.push_back(_backwardVisited[id].parentId);
    }

    // Merge and convert to Document
    for (auto& val : forwardPath) {
        result.emplace_back(Document{{"_id", val}});
    }
    result.emplace_back(Document{{"_id", meetingId}});  // Add the meeting node
    for (auto& val : backwardPath) {
        result.emplace_back(Document{{"_id", val}});
    }

    return result;
}

 BSONObj DocumentSourceBidirectionalGraphLookup::makeMatchStageFromFrontier(
     const ValueFlatUnorderedSet& frontier) {
     BSONObjBuilder match;
     BSONObjBuilder query(match.subobjStart("$match"));
     BSONArrayBuilder in(query.subarrayStart("_id"));
     
     for (auto&& value : frontier) {
         in << value;
     }
     
     in.done();
     query.done();
     
     return match.obj();
 }
 
 std::unique_ptr<Pipeline, PipelineDeleter> DocumentSourceBidirectionalGraphLookup::buildPipeline(
    const BSONObj& match) {
    std::vector<BSONObj> pipelineBson = {match};

    MakePipelineOptions pipelineOpts;
    pipelineOpts.optimize = true;
    pipelineOpts.attachCursorSource = true;

    if (_fromNs.isEmpty()) {
        LOGV2_WARNING(9999998, "Warning: _fromNs is empty at buildPipeline");
    } else {
        LOGV2(9999999, "Pipeline built for $lookup fromNs: {ns}", 
              "ns"_attr = _fromNs.toStringForErrorMsg());
    }

    auto resolvedNs = pExpCtx->getResolvedNamespace(_fromNs);
    auto expCtx = pExpCtx->copyWith(_fromNs, resolvedNs.uuid);

    return Pipeline::makePipeline(pipelineBson, expCtx, pipelineOpts);

}

 
 void DocumentSourceBidirectionalGraphLookup::checkMemoryUsage() {
     uassert(ErrorCodes::ExceededMemoryLimit,
             "$bidirectionalGraphLookup reached maximum memory consumption",
             (_visitedUsageBytes + _frontierUsageBytes) < _maxMemoryUsageBytes);
 }
 
 void DocumentSourceBidirectionalGraphLookup::resetSearchState() {
     _forwardVisited.clear();
     _backwardVisited.clear();
     _forwardFrontier.clear();
     _backwardFrontier.clear();
     _results.clear();
     _visitedUsageBytes = 0;
     _frontierUsageBytes = 0;
 }
 
 void DocumentSourceBidirectionalGraphLookup::doDispose() {
     resetSearchState();
 }
 
 boost::intrusive_ptr<DocumentSource> DocumentSourceBidirectionalGraphLookup::clone(
     const boost::intrusive_ptr<ExpressionContext>& newExpCtx) const {
     // Instead of having a separate copy constructor, we'll serialize and parse
     // the document source to create a proper clone
     auto cloned = make_intrusive<DocumentSourceBidirectionalGraphLookup>(
         newExpCtx,
         _fromNs,
         _as,
         _connectFromField,
         _connectToField,
         _startWith ? _startWith->optimize() : boost::intrusive_ptr<Expression>(),
         _target ? _target->optimize() : boost::intrusive_ptr<Expression>(),
         _additionalFilter,
         _depthField,
         _maxDepth);
     
     return cloned;
 }
 
 Value DocumentSourceBidirectionalGraphLookup::serialize(const SerializationOptions& opts) const {
     MutableDocument container;
     MutableDocument spec;
 
     spec["from"] = Value(NamespaceStringUtil::serialize(_fromNs, SerializationContext::stateCommandRequest()));
     spec["connectFromField"] = Value(_connectFromField);
     spec["connectToField"] = Value(_connectToField);
     spec["as"] = Value(_as);
     spec["startWith"] = _startWith->serialize(opts);
     spec["target"] = _target->serialize(opts);
     
     if (_additionalFilter) {
         spec["restrictSearchWithMatch"] = Value(*_additionalFilter);
     }
     if (_depthField) {
         spec["depthField"] = Value(*_depthField);
     }
     if (_maxDepth) {
         spec["maxDepth"] = Value(*_maxDepth);
     }
 
     container[getSourceName()] = Value(spec.freeze());
     return container.freezeToValue();
 }
 
 void DocumentSourceBidirectionalGraphLookup::serializeToArray(
     std::vector<Value>& array, const SerializationOptions& opts) const {
     array.push_back(serialize(opts));
 }
 
 DocumentSource::GetModPathsReturn DocumentSourceBidirectionalGraphLookup::getModifiedPaths() const {
     return {GetModPathsReturn::Type::kFiniteSet, {_as}, {}};
 }
 
 StageConstraints DocumentSourceBidirectionalGraphLookup::constraints(
     Pipeline::SplitState pipeState) const {
     return StageConstraints(StreamType::kStreaming,
                             PositionRequirement::kNone,
                             HostTypeRequirement::kNone,
                             DiskUseRequirement::kNoDiskUse,
                             FacetRequirement::kAllowed,
                             TransactionRequirement::kAllowed,
                             LookupRequirement::kAllowed,
                             UnionRequirement::kAllowed);
 }
 
 boost::optional<DocumentSource::DistributedPlanLogic> 
 DocumentSourceBidirectionalGraphLookup::distributedPlanLogic() {
     return boost::none;
 }
 
 boost::optional<ShardId> DocumentSourceBidirectionalGraphLookup::computeMergeShardId() const {
     return boost::none;
 }
 
 void DocumentSourceBidirectionalGraphLookup::detachFromOperationContext() {
     _fromExpCtx->setOperationContext(nullptr);
 }
 
 void DocumentSourceBidirectionalGraphLookup::reattachToOperationContext(OperationContext* opCtx) {
     _fromExpCtx->setOperationContext(opCtx);
 }
 
 bool DocumentSourceBidirectionalGraphLookup::validateOperationContext(const OperationContext* opCtx) const {
     return getContext()->getOperationContext() == opCtx &&
            _fromExpCtx->getOperationContext() == opCtx;
 }
 
 void DocumentSourceBidirectionalGraphLookup::addVariableRefs(std::set<Variables::Id>* refs) const {
     expression::addVariableRefs(_startWith.get(), refs);
     expression::addVariableRefs(_target.get(), refs);
 }
 
 DepsTracker::State DocumentSourceBidirectionalGraphLookup::getDependencies(DepsTracker* deps) const {
     expression::addDependencies(_startWith.get(), deps);
     expression::addDependencies(_target.get(), deps);
     return DepsTracker::State::SEE_NEXT;
 }
 
 void DocumentSourceBidirectionalGraphLookup::addInvolvedCollections(
     stdx::unordered_set<NamespaceString>* collectionNames) const {
     collectionNames->insert(_fromNs);
 }
 
 }  // namespace mongo