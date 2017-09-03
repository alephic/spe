#include "logic.h"

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

  void ValTree::add_(std::vector<ValPtr>::iterator it, std::vector<ValPtr>::iterator end, ValPtr& p) {
    if (it+1 == end) {
      this->leaves[*it] = p;
    } else {
      if (!this->branches.count(*it)) {
        this->branches[*it] = std::shared_ptr<ValTree>(new ValTree());
      }
      this->branches[*it]->add_(it+1, end, p);
    }
  }
  ValTree::ValTree() {}
  void ValTree::add(ValPtr& p) {
    std::vector<ValPtr> v;
    p->flatten(v);
    this->add_(v.begin(), v.end(), p);
  }
  void ValTree::get_matches(std::vector<ValPtr>::iterator it, std::vector<ValPtr>::iterator end, Scope b, std::vector<std::pair<ValPtr, Scope>>& out) const {
    if (it+1 == end) {
      for (const std::pair<const ValPtr, ValPtr>& leaf : this->leaves) {
        Scope s = Scope(&b);
        if ((*it)->match(leaf.first, s)) {
          out.push_back(std::pair<ValPtr, Scope>{leaf.second, s.squash()});
        }
      }
    } else {
      for (const std::pair<const ValPtr, std::shared_ptr<ValTree>>& branch : this->branches) {
        Scope s = Scope(&b);
        if ((*it)->match(branch.first, s)) {
          branch.second->get_matches(it+1, end, s, out);
        }
      }
    }
  }

  void World::get_matches_(std::vector<ValPtr>& valFlat, Scope &s, std::vector<std::pair<ValPtr, Scope>>& out) const {
    if (this->base != nullptr) {
      this->base->get_matches_(valFlat, s, out);
    }
    this->data.get_matches(valFlat.begin(), valFlat.end(), s, out);
  }
  World::World() : base{nullptr} {}
  World::World(const World *base) : base{base} {}
  void World::add(ValPtr& p) {
    this->data.add(p);
  }
  std::vector<std::pair<ValPtr, Scope>> World::get_matches(const ValPtr &p, Scope &s) const {
    std::vector<ValPtr> flat;
    p->flatten(flat);
    std::vector<std::pair<ValPtr, Scope>> v;
    this->get_matches_(flat, s, v);
    return v;
  }

  ValPtr bundle(Value *val) {
    ValPtr p(val);
    val->self = ValPtrWeak(p);
    return p;
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
      return s.get(this->ref_id);;
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
      if (this->ref_id == s->ref_id) {
        return true;
      }
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
  ValSet Arbitrary::eval(Scope& s, const World& w) {
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
  ValSet Apply::eval(Scope& s, const World& w) {
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
  bool Apply::operator==(const Value& other) const {
    if (const Apply *s = dynamic_cast<const Apply *>(&other)) {
      if (*this->pred == *s->pred && *this->arg == *s->arg) {
        return true;
      }
    }
    return false;
  }
  std::size_t Apply::hash() const {
    return 9858124 ^ this->pred->hash() ^ this->arg->hash();
  }
  void Apply::flatten(std::vector<ValPtr>& v) const {
    this->pred->flatten(v);
    this->arg->flatten(v);
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
  ValSet Declare::eval(Scope& s, const World& w) {
    ValSet withVals = this->with->eval(s, w);
    World w2 = World(&w);
    for (ValPtr withVal : withVals) {
      w2.add(withVal);
    }
    return this->body->eval(s, w2);
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
  ValSet Constrain::eval(Scope& s, const World& w) {
    Scope s2 = Scope(&s);
    Scope s3 = Scope(&s);
    std::unordered_set<SymId> refIds;
    this->constraint->collectRefIds(refIds);
    for (const SymId& refId : refIds) {
      ValSet refSet({bundle(new Ref(refId))}, 1);
      s2.add(refId, refSet);
      ValSet empty;
      s3.add(refId, empty);
    }
    ValSet constraintVals = this->constraint->eval(s2, w);
    bool has_match = false;
    for (const ValPtr& constraintVal : constraintVals) {
      for (const std::pair<ValPtr, Scope>& match : w.get_matches(constraintVal, s)) {
        has_match = true;
        for (const std::pair<SymId, ValSet>& binding : match.second.data) {
          if (refIds.count(binding.first)) {
            for (const ValPtr& boundVal : binding.second) {
              s3.data[binding.first].insert(boundVal);
            }
          }
        }
      }
    }
    if (has_match) {
      return this->body->eval(s3, w);
    }
    return ValSet();
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
