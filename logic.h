#ifndef __SPE_LOGIC_H
#define __SPE_LOGIC_H

#include <string>
#include <memory>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace logic {
  
  class Value;

  typedef std::shared_ptr<Value> ValPtr;

  struct ValPtrEqual {
    bool operator()(const ValPtr& v1, const ValPtr& v2) const;
  };

  struct ValPtrHash {
    std::size_t operator()(ValPtr const& v) const;
  };
  
  typedef std::weak_ptr<Value> ValPtrWeak;
  
  typedef std::string SymId;
  typedef std::unordered_set<ValPtr, ValPtrHash, ValPtrEqual> ValSet;

  class Scope {
  public:
    Scope *base;
    std::unordered_map<SymId, ValSet> data;
    Scope();
    Scope(Scope *base);
    void add(const SymId& k, ValSet& vs);
    ValSet& get(const SymId& k);
    virtual bool has(const SymId& k) const;
    virtual void squash_(std::unordered_map<SymId, ValSet>& out);
    Scope squash();
  };

  class Shadow : public Scope {
  protected:
    std::unordered_set<SymId> shadowed;
  public:
    Shadow(Scope *base);
    void shadow(const SymId& k);
    bool has(const SymId& k) const override;
    void squash_(std::unordered_map<SymId, ValSet>& out) override;
  };

  class World;

  class ValTable {
  private:
    std::unordered_map<ValPtr, std::shared_ptr<ValTable>, ValPtrHash, ValPtrEqual> branches;
    std::unordered_map<ValPtr, ValPtr, ValPtrHash, ValPtrEqual> leaves;
    std::vector<std::pair<ValPtr, std::shared_ptr<ValTable>>> quantified_branches;
    std::vector<std::pair<ValPtr, ValPtr>> quantified_leaves;
    void add_(std::vector<ValPtr>::iterator it, std::vector<ValPtr>::iterator end, const ValPtr& p);
  public:
    ValTable();
    void add(const ValPtr& p);
    void get_matches(std::vector<ValPtr>::iterator it, std::vector<ValPtr>::iterator end, Scope a, Scope b, World& w, std::vector<std::pair<ValPtr, Scope>>& out);
  };

  class CheckStep {
  public:
    ValPtr goal;
    ValPtr chosenDecl;
    CheckStep(const ValPtr& goal, const ValPtr& chosenDecl);
    bool operator==(const CheckStep& other) const;
  };

  class World {
  private:
    ValTable data;
    World *base;
    std::size_t numPrevSteps;
    std::vector<CheckStep> stepsTaken;
    std::size_t getNumStepsTaken() const;
    void get_matches_(std::vector<ValPtr>& valFlat, std::vector<std::pair<ValPtr, Scope>>& out);
    bool hasRepeatedStepSeq(std::vector<CheckStep>& seen, std::vector<CheckStep>& currMatch, std::size_t cutoff) const;
  public:
    World();
    World(World *base);
    void add(const ValPtr& p);
    std::vector<std::pair<ValPtr, Scope>> get_matches(ValPtr &p);
    bool isLegal(const CheckStep& next) const;
    void pushStep(const CheckStep& step);
    void popStep();
  };
  
  class Value {
  public:
    ValPtrWeak self;
    virtual void repr(std::ostream&) const = 0;
    virtual void repr_closed(std::ostream& o) const {this->repr(o);}
    virtual std::string repr_str() const;
    virtual ValSet subst(Scope&) = 0;
    virtual ValSet eval(Scope& s, World& w) {return this->subst(s);}
    virtual bool match(const ValPtr& other, Scope&) const {return *this == *other;}
    virtual bool operator==(const Value&) const = 0;
    virtual std::size_t hash() const = 0;
    virtual void flatten(std::vector<ValPtr>& v) const {v.push_back(this->self.lock());}
    virtual void collectRefIds(std::unordered_set<SymId>& s) const {}
  };

  ValPtr bundle(Value *val);

  class Sym: public Value {
  private:
    const SymId sym_id;
  public:
    Sym(const SymId &sym_id);
    void repr(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
  };
  
  class Wildcard: public Value {
  public:
    static ValPtr INSTANCE;
    Wildcard();
    void repr(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
  };

  class Ref : public Value {
  private:
    const SymId ref_id;
  public:
    Ref(const SymId& ref_id);
    void repr(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    bool match(const ValPtr& other, Scope& s) const override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
    void collectRefIds(std::unordered_set<SymId>& s) const override;
  };

  class Arbitrary: public Value {
  public:
    static ValPtr INSTANCE;
    Arbitrary();
    void repr(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    ValSet eval(Scope& s, World& w) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
  };

  class ArbitraryInstance: public Value {
  private:
    static std::size_t count;
    std::size_t id;
  public:
    ArbitraryInstance();
    void repr(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
  };

  class Lambda: public Value {
  private:
    static std::size_t count;
    std::size_t id;
    std::shared_ptr<std::unordered_set<SymId>> savedRefIds;
  public:
    const SymId arg_id;
    const ValPtr body;
    Lambda(const SymId& arg_id, const ValPtr& body);
    void repr(std::ostream& o) const override;
    void repr_closed(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
    void collectRefIds(std::unordered_set<SymId>& s) const override;
  };

  class Apply: public Value {
  private:
    std::shared_ptr<std::unordered_set<SymId>> savedRefIds;
  public:
    const ValPtr pred;
    const ValPtr arg;
    Apply(const ValPtr& pred, const ValPtr& arg);
    void repr(std::ostream& o) const override;
    void repr_closed(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    ValSet eval(Scope& s, World& w) override;
    bool match(const ValPtr& other, Scope& s) const override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
    void flatten(std::vector<ValPtr>& v) const override;
    void collectRefIds(std::unordered_set<SymId>& s) const override;
  };

  class Declare: public Value {
  private:
    std::shared_ptr<std::unordered_set<SymId>> savedRefIds;
  public:
    const ValPtr with;
    const ValPtr body;
    Declare(const ValPtr& with, const ValPtr& body);
    void repr(std::ostream& o) const override;
    void repr_closed(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    ValSet eval(Scope& s, World& w) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
    void collectRefIds(std::unordered_set<SymId>& s) const override;
  };

  class Constrain: public Value {
  private:
    std::shared_ptr<std::unordered_set<SymId>> savedRefIds;
  public:
    const ValPtr constraint;
    const ValPtr body;
    Constrain(const ValPtr& constraint, const ValPtr& body);
    void repr(std::ostream& o) const override;
    void repr_closed(std::ostream& o) const override;
    ValSet subst(Scope& s) override;
    ValSet eval(Scope& s, World& w) override;
    bool operator==(const Value& other) const override;
    std::size_t hash() const override;
    void collectRefIds(std::unordered_set<SymId>& s) const override;
  };
}

#endif
