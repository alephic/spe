#include "logic.h"
#include <sstream>

namespace logic {
  
  bool ValPtrEqual::operator()(const logic::ValPtr& v1, const logic::ValPtr& v2) const {
    return *v1 == *v2;
  }

  std::size_t ValPtrHash::operator()(logic::ValPtr const& v) const {
    return v->hash();
  }

  ValSet EMPTY;
  Scope::Scope() : base{nullptr} {}
  Scope::Scope(Scope *base) : base{base} {}
  void Scope::add(const SymId& k, ValSet& vs) {
    this->data[k] = vs;
  }
  ValSet& Scope::get(const SymId& k) {
    if (this->data.count(k)) {
      return this->data.at(k);
    } else if (this->base) {
      return this->base->get(k);
    } else {
      return EMPTY;
    }
  }
  bool Scope::has(const SymId& k) const {
    return this->data.count(k) || (this->base != nullptr && this->base->has(k));
  }
  void Scope::squash_(std::unordered_map<SymId, ValSet>& out) {
    if (this->base != nullptr) {
      this->base->squash_(out);
    }
    for (const std::pair<const SymId, ValSet>& kv : this->data) {
      out[kv.first] = kv.second;
    }
  }
  Scope Scope::squash() {
    Scope s;
    this->squash_(s.data);
    return s;
  }

  Shadow::Shadow(Scope *base): Scope(base) {}
  void Shadow::shadow(const SymId& k) {
    this->shadowed.insert(k);
  }
  bool Shadow::has(const SymId& k) const {
    return this->data.count(k) || ((!this->shadowed.count(k)) && this->base->has(k));
  }
  void Shadow::squash_(std::unordered_map<SymId, ValSet>& out) {
    if (this->base != nullptr) {
      this->base->squash_(out);
    }
    for (const SymId& k : this->shadowed) {
      out.erase(k);
    }
    for (const std::pair<const SymId, ValSet>& kv : this->data) {
      out[kv.first] = kv.second;
    }
  }

  ValTable::ValTable() {}
  void ValTable::add_(std::vector<ValPtr>::iterator it, std::vector<ValPtr>::iterator end, const ValPtr& p) {
    std::unordered_set<SymId> refIds;
    (*it)->collectRefIds(refIds);
    if (refIds.size() > 0) {
      if (it+1 == end) {
        this->quantified_leaves.push_back(std::pair<ValPtr, ValPtr>(*it, p));
      } else {
        std::shared_ptr<ValTable> vtp = std::shared_ptr<ValTable>(new ValTable());
        vtp->add_(it+1, end, p);
        this->quantified_branches.push_back(std::pair<ValPtr, std::shared_ptr<ValTable>>(*it, vtp));
      }
    } else {
      if (it+1 == end) {
        this->leaves[*it] = p;
      } else {
        if (!this->branches.count(*it)) {
          this->branches[*it] = std::shared_ptr<ValTable>(new ValTable());
        }
        this->branches[*it]->add_(it+1, end, p);
      }
    }
  }
  ValPtr stripLambdas(const ValPtr& p) {
    if (const Lambda *l = dynamic_cast<const Lambda *>(p.get())) {
      return stripLambdas(l->body);
    } else if (const Declare *d = dynamic_cast<const Declare *>(p.get())) {
      ValPtr newBody = stripLambdas(d->body);
      if (newBody != d->body)
        return bundle(new Declare(d->with, newBody));
      else
        return p;
    } else if (const Constrain *c = dynamic_cast<const Constrain *>(p.get())) {
      ValPtr newBody = stripLambdas(c->body);
      if (newBody != c->body)
        return bundle(new Constrain(c->constraint, newBody));
      else
        return p;
    } else {
      return p;
    }
  }
  ValPtr extractApply(const ValPtr& p) {
    if (const Lambda *l = dynamic_cast<const Lambda *>(p.get())) {
      return extractApply(l->body);
    } else if (const Declare *d = dynamic_cast<const Declare *>(p.get())) {
      return extractApply(d->body);
    } else if (const Constrain *c = dynamic_cast<const Constrain *>(p.get())) {
      return extractApply(c->body);
    } else {
      return p;
    }
  }
  Value& getVal(const ValPtr& vp) {
    return *vp;
  }
  std::unordered_set<SymId> getRefIds(const ValPtr& vp) {
    std::unordered_set<SymId> refIds;
    vp->collectRefIds(refIds);
    return refIds;
  }
  void ValTable::add(const ValPtr& p) {
    std::vector<ValPtr> v;
    ValPtr p2 = stripLambdas(p);
    extractApply(p2)->flatten(v);
    this->add_(v.begin(), v.end(), p2);
  }
  void ValTable::get_matches_whole_val(const ValPtr& val, Scope a, Scope b, World& w, std::vector<std::pair<ValPtr, Scope>>& out) {
    for (const std::pair<const ValPtr, ValPtr>& leaf : this->leaves) {
      CheckStep next = CheckStep(val, leaf.second);
      if (w.isLegal(next)) {
        w.pushStep(next);
        Scope a2(&a);
        if (val->match(leaf.first, a2) && leaf.second->eval(b, w).size() > 0) {
          out.push_back(std::pair<ValPtr, Scope>{leaf.second, a2.squash()});
        }
        w.popStep();
      }
    }
    for (const std::pair<const ValPtr, ValPtr>& leaf : this->quantified_leaves) {
      CheckStep next = CheckStep(val, leaf.second);
      if (w.isLegal(next)) {
        w.pushStep(next);
        Scope b2(&b);
        if (leaf.first->match(val, b2) && leaf.second->eval(b2, w).size() > 0) {
          out.push_back(std::pair<ValPtr, Scope>{leaf.second, a.squash()});
        }
        w.popStep();
      }
    }
    for (const std::pair<const ValPtr, std::shared_ptr<ValTable>>& branch : this->branches) {
      branch.second->get_matches_whole_val(val, a, b, w, out);
    }
    for (const std::pair<const ValPtr, std::shared_ptr<ValTable>>& branch : this->quantified_branches) {
      branch.second->get_matches_whole_val(val, a, b, w, out);
    }
  }
  void ValTable::get_matches(const ValPtr& val, std::vector<ValPtr>::iterator it, std::vector<ValPtr>::iterator end, Scope a, Scope b, World& w, std::vector<std::pair<ValPtr, Scope>>& out) {
    std::size_t numRefs = getRefIds(*it).size();
    if (it+1 == end) {
      bool exactMatched = false;
      if (numRefs == 0 && this->leaves.count(*it)) {
        CheckStep next = CheckStep(val, this->leaves[*it]);
        if (w.isLegal(next)) {
          w.pushStep(next);
          if (this->leaves[*it]->eval(b, w).size() > 0) {
            exactMatched = true;
            out.push_back(std::pair<ValPtr, Scope>{this->leaves[*it], a.squash()});
          }
          w.popStep();
        }
      }
      if (!exactMatched && numRefs > 0) {
        for (const std::pair<const ValPtr, ValPtr>& leaf : this->leaves) {
          CheckStep next = CheckStep(val, leaf.second);
          if (w.isLegal(next)) {
            w.pushStep(next);
            Scope a2(&a);
            if ((*it)->match(leaf.first, a2) && leaf.second->eval(b, w).size() > 0) {
              out.push_back(std::pair<ValPtr, Scope>{leaf.second, a2.squash()});
            }
            w.popStep();
          }
        }
      }
      for (const std::pair<const ValPtr, ValPtr>& leaf : this->quantified_leaves) {
        CheckStep next = CheckStep(val, leaf.second);
        if (w.isLegal(next)) {
          w.pushStep(next);
          Scope b2(&b);
          if (leaf.first->match(*it, b2) && leaf.second->eval(b2, w).size() > 0) {
            out.push_back(std::pair<ValPtr, Scope>{leaf.second, a.squash()});
          }
          w.popStep();
        }
      }
    } else {
      if (numRefs == 0 && this->branches.count(*it)) {
        this->branches[*it]->get_matches(val, it+1, end, a, b, w, out);
      } else if (numRefs > 0) {
        for (const std::pair<const ValPtr, std::shared_ptr<ValTable>>& branch : this->branches) {
          Scope a2(&a);
          if ((*it)->match(branch.first, a2)) {
            branch.second->get_matches(val, it+1, end, a2, b, w, out);
          }
        }
      }
      for (const std::pair<const ValPtr, std::shared_ptr<ValTable>>& branch : this->quantified_branches) {
        Scope b2(&b);
        if (branch.first->match(*it, b2)) {
          branch.second->get_matches(val, it+1, end, a, b2, w, out);
        }
      }
    }
  }

  CheckStep::CheckStep(const ValPtr& goal, const ValPtr& chosenDecl) : goal(goal), chosenDecl(chosenDecl) {}
  bool CheckStep::operator==(const CheckStep& other) const {
    //return (*other.goal) == (*this->goal) && other.chosenDecl.get() == this->chosenDecl.get();
    return other.chosenDecl.get() == this->chosenDecl.get();
  }

  std::size_t World::getNumStepsTaken() const {
    return this->stepsTaken.size();
  }
  bool World::hasRepeatedStepSeq(std::vector<CheckStep>& seen, std::vector<CheckStep>& currMatch, std::size_t cutoff) const {
    if (seen.size() >= cutoff) {
      return false;
    }
    for (std::size_t i = 1; i <= this->stepsTaken.size(); ++i) {
      const CheckStep& curr = this->stepsTaken[this->stepsTaken.size() - i];
      seen.push_back(curr);
      if (seen.size() > 1) {
        if (curr == seen[currMatch.size()]) {
          currMatch.push_back(curr);
          if (currMatch.size()*2 == seen.size()) {
            return true;
          }
        } else {
          currMatch.clear();
        }
      }
    }
    if (this->base != nullptr) {
      return this->base->hasRepeatedStepSeq(seen, currMatch, cutoff);
    } else {
      return false;
    }
  }
  World::World() : base{nullptr}, numPrevSteps{0} {}
  World::World(World *base) : base{base}, numPrevSteps{base ? base->getNumStepsTaken() : 0} {}
  void World::add(const ValPtr& p) {
    this->data.add(p);
  }
  std::vector<std::pair<ValPtr, Scope>> World::get_matches(ValPtr &p) {
    std::vector<ValPtr> flat;
    p->flatten(flat);
    std::vector<std::pair<ValPtr, Scope>> res;
    for (World *curr = this; curr != nullptr; curr = curr->base) {
      curr->data.get_matches(p, flat.begin(), flat.end(), Scope(), Scope(), *curr, res);
    }
    if (flat.size() > 1) {
      std::vector<ValPtr> single({p});
      for (World *curr = this; curr != nullptr; curr = curr->base) {
        curr->data.get_matches(p, single.begin(), single.end(), Scope(), Scope(), *curr, res);
      }
    } else if (getRefIds(p).size() > 0) {
      for (World *curr = this; curr != nullptr; curr = curr->base) {
        curr->data.get_matches_whole_val(p, Scope(), Scope(), *curr, res);
      }
    }
    return res;
  }
  bool World::isLegal(const CheckStep& next) const {
    std::vector<CheckStep> seen({next});
    std::vector<CheckStep> match;
    return !this->hasRepeatedStepSeq(seen, match, (this->getNumStepsTaken() + 1) / 2 + 1);
  }
  void World::pushStep(const CheckStep& step) {
    this->stepsTaken.push_back(step);
  }
  void World::popStep() {
    this->stepsTaken.pop_back();
  }

  ValPtr bundle(Value *val) {
    ValPtr p(val);
    val->self = ValPtrWeak(p);
    return p;
  }

  std::string Value::repr_str() const {
    std::stringstream sstr;
    this->repr(sstr);
    return sstr.str();
  }

  Sym::Sym(const SymId &sym_id) : sym_id(sym_id) {}
  void Sym::repr(std::ostream& o) const {
    o << this->sym_id;
  }
  ValSet Sym::subst(Scope& s) {
    return ValSet({this->self.lock()}, 1);
  }
  bool Sym::operator==(const Value& other) const {
    if (const Sym *s = dynamic_cast<const Sym *>(&other)) {
      if (this->sym_id == s->sym_id) {
        return true;
      }
    }
    return false;
  }
  std::size_t Sym::hash() const {
    return 85831957 ^ std::hash<std::string>{}(this->sym_id);
  }
  
  Wildcard::Wildcard() {}
  void Wildcard::repr(std::ostream& o) const {
    o << '*';
  }
  ValSet Wildcard::subst(Scope& s) {
    return ValSet({this->self.lock()}, 1);
  }
  bool Wildcard::operator==(const Value& other) const {
    if (const Wildcard *s = dynamic_cast<const Wildcard *>(&other)) {
      return true;
    }
    return false;
  }
  std::size_t Wildcard::hash() const {
    return 12952153;
  }

  ValPtr Wildcard::INSTANCE(bundle(new Wildcard()));
  
  Ref::Ref(const SymId& ref_id) : ref_id(ref_id) {}
  void Ref::repr(std::ostream& o) const {
    o << this->ref_id;
  }
  ValSet Ref::subst(Scope& s) {
    if (s.has(this->ref_id)) {
      ValSet& vs = s.get(this->ref_id);
      if (!vs.count(Wildcard::INSTANCE))
        return vs;
    }
    return ValSet({this->self.lock()}, 1);
  }
  bool Ref::match(const ValPtr& other, Scope& s) const {
    if (s.has(this->ref_id)) {
      const ValSet& vs = s.get(this->ref_id);
      if (vs.count(other) || vs.count(Wildcard::INSTANCE)) {
        ValSet vs2({other}, 1);
        s.add(this->ref_id, vs2);
        return true;
      } else {
        return false;
      }
    } else {
      ValSet vs({other}, 1);
      s.add(this->ref_id, vs);
      return true;
    }
  }
  bool Ref::operator==(const Value& other) const {
    if (const Ref *s = dynamic_cast<const Ref *>(&other)) {
      return this->ref_id == s->ref_id;
    }
    return false;
  }
  std::size_t Ref::hash() const {
    return 128582195 ^ std::hash<std::string>{}(this->ref_id);
  }
  void Ref::collectRefIds(std::unordered_set<SymId>& s) const {
    s.insert(this->ref_id);
  }

  Arbitrary::Arbitrary() {}
  void Arbitrary::repr(std::ostream& o) const {
    o << '?';
  }
  ValSet Arbitrary::subst(Scope& s) {
    return ValSet({this->self.lock()}, 1);
  }
  ValSet Arbitrary::eval(Scope& s, World& w) {
    return ValSet({bundle(new ArbitraryInstance())}, 1);
  }
  bool Arbitrary::operator==(const Value& other) const {
    if (const Arbitrary *s = dynamic_cast<const Arbitrary *>(&other)) {
      return true;
    }
    return false;
  }
  std::size_t Arbitrary::hash() const {
    return 95318557;
  }

  ValPtr Arbitrary::INSTANCE(bundle(new Arbitrary()));

  std::size_t ArbitraryInstance::count(0);
  ArbitraryInstance::ArbitraryInstance() {
    this->id = count;
    ++count;
  }
  void ArbitraryInstance::repr(std::ostream& o) const {
    o << '?' << id;
  }
  ValSet ArbitraryInstance::subst(Scope& s) {
    return ValSet({this->self.lock()}, 1);
  }
  bool ArbitraryInstance::operator==(const Value& other) const {
    if (const ArbitraryInstance *s = dynamic_cast<const ArbitraryInstance *>(&other)) {
      if (this->id == s->id) {
        return true;
      }
    }
    return false;
  }
  std::size_t ArbitraryInstance::hash() const {
    return 998439321 ^ this->id;
  }

  std::size_t Lambda::count(0);
  Lambda::Lambda(const SymId& arg_id, const ValPtr& body) : arg_id(arg_id), body(body) {
    this->id = count;
    ++count;
  }
  void Lambda::repr(std::ostream& o) const {
    o << '<' << this->arg_id << '>' << ' ';
    this->body->repr(o);
  }
  void Lambda::repr_closed(std::ostream& o) const {
    o << '(';
    this->repr(o);
    o << ')';
  }
  ValSet Lambda::subst(Scope& s) {
    if (!this->savedRefIds) {
      this->savedRefIds = std::shared_ptr<std::unordered_set<SymId>>(new std::unordered_set<SymId>());
      this->collectRefIds(*this->savedRefIds);
    }
    bool disjoint = true;
    for (const SymId& refId : *this->savedRefIds) {
      if (s.has(refId)) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      return ValSet({this->self.lock()}, 1);
    }
    Shadow sh = Shadow(&s);
    sh.shadow(this->arg_id);
    ValSet bodySubstdVals = this->body->subst(sh);
    ValSet res(bodySubstdVals.bucket_count());
    for (const ValPtr& bodySubstd : bodySubstdVals) {
      res.insert(bundle(new Lambda(this->arg_id, bodySubstd)));
    }
    return res;
  }
  bool Lambda::operator==(const Value& other) const {
    if (const Lambda *s = dynamic_cast<const Lambda *>(&other)) {
      if (this->id == s->id) {
        return true;
      }
    }
    return false;
  }
  std::size_t Lambda::hash() const {
    return 195218521 ^ this->id;
  }
  void Lambda::collectRefIds(std::unordered_set<SymId>& refIds) const {
    if (refIds.count(this->arg_id)) {
      this->body->collectRefIds(refIds);
    } else {
      this->body->collectRefIds(refIds);
      refIds.erase(this->arg_id);
    }
  }

  Apply::Apply(const ValPtr& pred, const ValPtr& arg) : pred(pred), arg(arg) {}
  void Apply::repr(std::ostream& o) const {
    if (const Apply *s = dynamic_cast<const Apply *>(this->pred.get())) {
      this->pred->repr(o);
      o << ' ';
      this->arg->repr_closed(o);
    } else {
      this->pred->repr_closed(o);
      o << ' ';
      this->arg->repr_closed(o);
    }
  }
  void Apply::repr_closed(std::ostream& o) const {
    o << '(';
    this->repr(o);
    o << ')';
  }
  ValSet Apply::subst(Scope& s) {
    if (!this->savedRefIds) {
      this->savedRefIds = std::shared_ptr<std::unordered_set<SymId>>(new std::unordered_set<SymId>());
      this->collectRefIds(*this->savedRefIds);
    }
    bool disjoint = true;
    for (const SymId& refId : *this->savedRefIds) {
      if (s.has(refId)) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      return ValSet({this->self.lock()}, 1);
    }
    ValSet predVals = this->pred->subst(s);
    ValSet argVals = this->arg->subst(s);
    ValSet res(predVals.bucket_count()*argVals.bucket_count());
    for (const ValPtr& predVal : predVals) {
      for (const ValPtr& argVal : argVals) {
        res.insert(bundle(new Apply(predVal, argVal)));
      }
    }
    return res;
  }
  ValSet Apply::eval(Scope& s, World& w) {
    ValSet predVals = this->pred->eval(s, w);
    ValSet argVals = this->arg->eval(s, w);
    ValSet res(predVals.bucket_count()*argVals.bucket_count());
    for (const ValPtr& predVal : predVals) {
      if (const Lambda *l = dynamic_cast<const Lambda *>(predVal.get())) {
        Scope s2 = Scope(&s);
        s2.add(l->arg_id, argVals);
        for (const ValPtr& resVal : l->body->eval(s2, w)) {
          res.insert(resVal);
        }
      } else {
        for (const ValPtr& argVal : argVals) {
          res.insert(bundle(new Apply(predVal, argVal)));
        }
      }
    }
    return res;
  }
  bool Apply::match(const ValPtr& other, Scope& s) const {
    if (const Apply *a = dynamic_cast<const Apply *>(other.get())) {
      return this->pred->match(a->pred, s) && this->arg->match(a->arg, s);
    }
    return false;
  }
  bool Apply::operator==(const Value& other) const {
    if (const Apply *s = dynamic_cast<const Apply *>(&other)) {
      return *this->pred == *s->pred && *this->arg == *s->arg;
    }
    return false;
  }
  std::size_t Apply::hash() const {
    return 9858124 ^ this->pred->hash() ^ this->arg->hash();
  }
  void Apply::flatten(std::vector<ValPtr>& v) const {
    this->pred->flatten(v);
    v.push_back(this->arg);
  }
  void Apply::collectRefIds(std::unordered_set<SymId>& refIds) const {
    this->pred->collectRefIds(refIds);
    this->arg->collectRefIds(refIds);
  }

  Declare::Declare(const ValPtr& with, const ValPtr& body) : with(with), body(body) {}
  void Declare::repr(std::ostream& o) const {
    o << '{';
    this->with->repr(o);
    o << '}' << ' ';
    this->body->repr(o);
  }
  void Declare::repr_closed(std::ostream& o) const {
    o << '(';
    this->repr(o);
    o << ')';
  }
  ValSet Declare::subst(Scope& s) {
    if (!this->savedRefIds) {
      this->savedRefIds = std::shared_ptr<std::unordered_set<SymId>>(new std::unordered_set<SymId>());
      this->collectRefIds(*this->savedRefIds);
    }
    bool disjoint = true;
    for (const SymId& refId : *this->savedRefIds) {
      if (s.has(refId)) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      return ValSet({this->self.lock()}, 1);
    }
    ValSet withVals = this->with->subst(s);
    ValSet bodyVals = this->body->subst(s);
    ValSet res(withVals.bucket_count()*bodyVals.bucket_count());
    for (const ValPtr& withVal : withVals) {
      for (const ValPtr& bodyVal : bodyVals) {
        res.insert(bundle(new Declare(withVal, bodyVal)));
      }
    }
    return res;
  }
  ValSet Declare::eval(Scope& s, World& w) {
    World w2(&w);
    ValSet withVals = this->with->eval(s, w);
    for (const ValPtr& withVal : withVals) {
      w2.add(withVal);
    }
    const Declare *prev = this;
    ValPtr curr = this->body;
    while (const Declare *d = dynamic_cast<const Declare *>(curr.get())) {
      withVals = d->with->eval(s, w);
      for (const ValPtr& withVal : withVals) {
        w2.add(withVal);
      }
      prev = d;
      curr = d->body;
    }
    return prev->body->eval(s, w2);
  }
  bool Declare::operator==(const Value& other) const {
    if (const Declare *s = dynamic_cast<const Declare *>(&other)) {
      if (*this->with == *s->with && *this->body == *s->body) {
        return true;
      }
    }
    return false;
  }
  std::size_t Declare::hash() const {
    return 2958125 ^ this->with->hash() ^ this->body->hash();
  }
  void Declare::collectRefIds(std::unordered_set<SymId>& refIds) const {
    this->with->collectRefIds(refIds);
    this->body->collectRefIds(refIds);
  }

  Constrain::Constrain(const ValPtr& constraint, const ValPtr& body) : constraint(constraint), body(body) {}
  void Constrain::repr(std::ostream& o) const {
    o << '[';
    this->constraint->repr(o);
    o << ']' << ' ';
    this->body->repr(o);
  }
  void Constrain::repr_closed(std::ostream& o) const {
    o << '(';
    this->repr(o);
    o << ')';
  }
  ValSet Constrain::subst(Scope& s) {
    if (!this->savedRefIds) {
      this->savedRefIds = std::shared_ptr<std::unordered_set<SymId>>(new std::unordered_set<SymId>());
      this->collectRefIds(*this->savedRefIds);
    }
    bool disjoint = true;
    for (const SymId& refId : *this->savedRefIds) {
      if (s.has(refId)) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      return ValSet({this->self.lock()}, 1);
    }
    ValSet constraintVals = this->constraint->subst(s);
    ValSet bodyVals = this->body->subst(s);
    ValSet res(constraintVals.bucket_count()*bodyVals.bucket_count());
    for (const ValPtr& constraintVal : constraintVals) {
      for (const ValPtr& bodyVal : bodyVals) {
        res.insert(bundle(new Constrain(constraintVal, bodyVal)));
      }
    }
    return res;
  }
  ValSet Constrain::eval(Scope& s, World& w) {
    std::unordered_set<SymId> refIds;
    this->constraint->collectRefIds(refIds);
    if (refIds.size() == 0) {
      for (ValPtr constraintVal : this->constraint->eval(s, w)) {
        if (w.get_matches(constraintVal).size() > 0) {
          return this->body->eval(s, w);
        }
      }
      return ValSet();
    } else {
      Scope s2 = Scope(&s);
      ValSet res;
      std::vector<std::pair<SymId, ValSet>> bindings;
      std::vector<ValSet::iterator> binding_iters;
      for (auto it = refIds.begin(); it != refIds.end(); ++it) {
        if (s.has(*it)) {
          bindings.push_back(std::pair<SymId, ValSet>(*it, s.get(*it)));
          binding_iters.push_back(bindings[bindings.size() - 1].second.begin());
          if (binding_iters[binding_iters.size() - 1] == bindings[bindings.size() - 1].second.end()) {
            s2.data[*it] = ValSet();
          } else {
            s2.data[*it] = ValSet({*binding_iters[binding_iters.size() - 1]}, 1);
          }
        }
      }
      if (bindings.size() == 0) {
        for (ValPtr constraintVal : this->constraint->eval(s2, w)) {
          bool scopelessMatch(false);
          for (std::pair<ValPtr, Scope>& match : w.get_matches(constraintVal)) {
            if (match.second.data.size() > 0) {
              Scope& s3 = match.second;
              s3.base = &s2;
              for (const ValPtr& bodyVal : this->body->eval(s3, w)) {
                res.insert(bodyVal);
              }
            } else if (!scopelessMatch) {
              scopelessMatch = true;
              for (const ValPtr& bodyVal : this->body->eval(s2, w)) {
                res.insert(bodyVal);
              }
            }
          }
        }
        return res;
      }
      std::size_t last_idx = binding_iters.size() - 1;
      std::size_t curr_idx;
      while (binding_iters[0] != bindings[0].second.end()) {
        for (ValPtr constraintVal : this->constraint->eval(s2, w)) {
          bool scopelessMatch(false);
          for (const std::pair<ValPtr, Scope>& match : w.get_matches(constraintVal)) {
            if (match.second.data.size() > 0) {
              Scope s3 = match.second;
              s3.base = &s2;
              for (const ValPtr& bodyVal : this->body->eval(s3, w)) {
                res.insert(bodyVal);
              }
            } else if (!scopelessMatch) {
              scopelessMatch = true;
              for (const ValPtr& bodyVal : this->body->eval(s2, w)) {
                res.insert(bodyVal);
              }
            }
          }
        }
        ++binding_iters[last_idx];
        if (binding_iters[last_idx] == bindings[last_idx].second.end()) {
          curr_idx = last_idx;
          while (binding_iters[curr_idx] == bindings[curr_idx].second.end() && curr_idx > 0) {
            binding_iters[curr_idx] = bindings[curr_idx].second.begin();
            s2.data[bindings[curr_idx].first].clear();
            s2.data[bindings[curr_idx].first].insert(*binding_iters[curr_idx]);
            --curr_idx;
            ++binding_iters[curr_idx];
          }
        } else {
          s2.data[bindings[last_idx].first].clear();
          s2.data[bindings[last_idx].first].insert(*binding_iters[last_idx]);
        }
      }
      return res;
    }
  }
  bool Constrain::operator==(const Value& other) const {
    if (const Constrain *s = dynamic_cast<const Constrain *>(&other)) {
      if (*this->constraint == *s->constraint && *this->body == *s->body) {
        return true;
      }
    }
    return false;
  }
  std::size_t Constrain::hash() const {
    return 28148592 ^ this->constraint->hash() ^ this->body->hash();
  }
  void Constrain::collectRefIds(std::unordered_set<SymId>& refIds) const {
    this->constraint->collectRefIds(refIds);
    this->body->collectRefIds(refIds);
  }
}
