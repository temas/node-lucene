#include <iostream>
#include <v8.h>
#include <node.h>
//Thanks bnoordhuis and jerrysv from #node.js

#include <sstream>

#include <CLucene.h>
#include "Misc.h"
#include "repl_tchar.h"
#include "StringBuffer.h"

using namespace node;
using namespace v8;
using namespace std;
using namespace lucene::index;
using namespace lucene::analysis;
using namespace lucene::util;
using namespace lucene::store;
using namespace lucene::document;
using namespace lucene::search;
using namespace lucene::queryParser;

const static int CL_MAX_DIR = 220;

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
      return ThrowException(Exception::TypeError(                       \
                  String::New("Argument " #I " must be a function")));  \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

class TCharWrapper : public String::ExternalStringResource {
public:
    TCharWrapper(const TCHAR* tcharToWrap) : String::ExternalStringResource(), tchar_(tcharToWrap)
    { }

    const uint16_t* data() const { return reinterpret_cast<const uint16_t*>(tchar_); }
    size_t length() const { return _tcslen(tchar_); }
private:
    const TCHAR* tchar_;
};

class LuceneDocument : public ObjectWrap {
public:
    static void Initialize(v8::Handle<v8::Object> target) {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(New);

        t->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "addField", AddField);

        target->Set(String::NewSymbol("Document"), t->GetFunction());
    }

    void setDocument(Document* doc) { doc_ = doc; }
    Document* document() const { return doc_; }
protected:
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        LuceneDocument* newDoc = new LuceneDocument();
        newDoc->Wrap(args.This());

        return scope.Close(args.This());
    }

    // args:
    //   String* key
    //   String* value
    //   Integer flags
    static Handle<Value> AddField(const Arguments& args) {
        HandleScope scope;
        TryCatch try_catch;

        LuceneDocument* docWrapper = ObjectWrap::Unwrap<LuceneDocument>(args.This());
        printf("Got the document unwrapped\n");

        TCHAR key[CL_MAX_DIR];
        STRCPY_AtoT(key, *String::Utf8Value(args[0]), CL_MAX_DIR);

        TCHAR value[CL_MAX_DIR];
        STRCPY_AtoT(value, *String::Utf8Value(args[1]), CL_MAX_DIR);

        _tprintf(_T("Going to add %S:%S:%d\n"), key, value, args[2]->Int32Value());
        Field* field = _CLNEW Field(key, value, args[2]->Int32Value());
        printf("Created the field\n");
        docWrapper->document()->add(*field);
        printf("Document added\n");
        
        if (try_catch.HasCaught()) {
            FatalException(try_catch);
        }
        
        return scope.Close(Undefined());
    }

    LuceneDocument() : ObjectWrap(), doc_(_CLNEW Document) {
        printf("Created new document %p\n", doc_);
    }

    ~LuceneDocument() {
        delete doc_;
        printf("Deleting a LuceneDocument\n");
    }
private:
    Document* doc_;
};

class Lucene : public ObjectWrap {

    static Persistent<FunctionTemplate> s_ct;
    
    private:
        int m_count;
        v8::Local<v8::Array> junk;
        
    public:

        static void Init(Handle<Object> target) {
            HandleScope scope;

            Local<FunctionTemplate> t = FunctionTemplate::New(New);

            s_ct = Persistent<FunctionTemplate>::New(t);
            s_ct->InstanceTemplate()->SetInternalFieldCount(1);
            s_ct->SetClassName(String::NewSymbol("Lucene"));

            NODE_SET_PROTOTYPE_METHOD(s_ct, "addDocument", AddDocumentAsync);
            NODE_SET_PROTOTYPE_METHOD(s_ct, "search", SearchAsync);

            target->Set(String::NewSymbol("Lucene"), s_ct->GetFunction());
        }
    
        Lucene() : m_count(0) {}

        ~Lucene() {}

        static Handle<Value> New(const Arguments& args) {
            HandleScope scope;
            Lucene* lucene = new Lucene();
            lucene->Wrap(args.This());
            return args.This();
        }
        
        struct index_baton_t {
            Lucene* lucene;         
            LuceneDocument* doc;
            v8::String::Utf8Value* index;
            Persistent<Function> callback;
            uint64_t indexTime;
        };
        
        // args:
        //   Document* doc
        //   String* indexPath
        static Handle<Value> AddDocumentAsync(const Arguments& args) {
            HandleScope scope;

            Lucene* lucene = ObjectWrap::Unwrap<Lucene>(args.This());
            REQ_FUN_ARG(2, callback);

            index_baton_t* baton = new index_baton_t;
            baton->lucene = lucene;
            baton->doc = ObjectWrap::Unwrap<LuceneDocument>(args[0]->ToObject());
            baton->index = new v8::String::Utf8Value(args[1]);
            baton->callback = Persistent<Function>::New(callback);

            lucene->Ref();

            eio_custom(EIO_Index, EIO_PRI_DEFAULT, EIO_AfterIndex, baton);
            ev_ref(EV_DEFAULT_UC);

            return scope.Close(Undefined());
        }
            
        
        static int EIO_Index(eio_req* req) {
            index_baton_t* baton = static_cast<index_baton_t*>(req->data);

            IndexWriter* writer = NULL;
            lucene::analysis::standard::StandardAnalyzer an;

            if (IndexReader::indexExists(*(*baton->index))) {
                if (IndexReader::isLocked(*(*baton->index))) {
                    printf("Index was locked... unlocking it.\n");
                    IndexReader::unlock(*(*baton->index));
                }
                writer = _CLNEW IndexWriter(*(*baton->index), &an, false);
            } else {
                writer = _CLNEW IndexWriter(*(*baton->index), &an, true);
            }
            
            printf("Setting writer to %s\n", *(*baton->index));
            // We can tell the writer to flush at certain occasions
            //writer->setRAMBufferSizeMB(0.5);
            //writer->setMaxBufferedDocs(3);

            // To bypass a possible exception (we have no idea what we will be indexing...)
            writer->setMaxFieldLength(0x7FFFFFFFL); // LUCENE_INT32_MAX_SHOULDBE
            // Turn this off to make indexing faster; we'll turn it on later before optimizing
            writer->setUseCompoundFile(false);
            uint64_t start = Misc::currentTimeMillis();

            try {
              writer->addDocument(baton->doc->document());
            } catch (CLuceneError& E) {
              printf("Got an exception: %s\n", E.what());
            } catch(...) {
              printf("Got an unknown exception\n");
            }

            printf("Done adding document\n");

            // Make the index use as little files as possible, and optimize it
            writer->setUseCompoundFile(true);
            writer->optimize();

            // Close and clean up
            writer->close();
            _CLLDELETE(writer);
            
            baton->indexTime = (Misc::currentTimeMillis() - start);
            
            printf("Indexing took: %d ms.\n\n", (uint32_t)(baton->indexTime));
            IndexWriter(*(*baton->index), &an, false);

            return 0;
        }

        static int EIO_AfterIndex(eio_req* req) {
            HandleScope scope;
            index_baton_t* baton = static_cast<index_baton_t*>(req->data);
            ev_unref(EV_DEFAULT_UC);
            baton->lucene->Unref();

            Handle<Value> argv[2];

            argv[0] = Null(); // Error arg, defaulting to no error
            argv[1] = v8::Integer::NewFromUnsigned((uint32_t)baton->indexTime);

            TryCatch tryCatch;

            baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

            if (tryCatch.HasCaught()) {
                FatalException(tryCatch);
            }

            baton->callback.Dispose();

            delete baton->index;
            delete baton;
            return 0;
        }
        

        struct search_baton_t
        {
            Lucene* lucene;
            v8::String::Utf8Value* index;
            v8::String::Utf8Value* search;
            Persistent<Function> callback;
            Persistent<v8::Array> results;
        };

        static Handle<Value> SearchAsync(const Arguments& args) {
            HandleScope scope;

            Lucene* lucene = ObjectWrap::Unwrap<Lucene>(args.This());
            REQ_FUN_ARG(2, callback);

            search_baton_t* baton = new search_baton_t;
            baton->lucene = lucene;
            baton->index = new v8::String::Utf8Value(args[0]);
            baton->search = new v8::String::Utf8Value(args[1]);
            baton->callback = Persistent<Function>::New(callback);

            lucene->Ref();

            eio_custom(EIO_Search, EIO_PRI_DEFAULT, EIO_AfterSearch, baton);
            ev_ref(EV_DEFAULT_UC);

            return scope.Close(Undefined());
        }

        static int EIO_Search(eio_req* req) 
        {
            search_baton_t* baton = static_cast<search_baton_t*>(req->data);

            standard::StandardAnalyzer analyzer;
            IndexReader* reader = IndexReader::open(*(*baton->index));
            IndexReader* newreader = reader->reopen();
            if ( newreader != reader ) {
                _CLLDELETE(reader);
                reader = newreader;
            }
            IndexSearcher s(reader);

            TCHAR* searchString = STRDUP_AtoT(*(*baton->search));
            Query* q = QueryParser::parse(searchString, _T(""), &analyzer);
            Hits* hits = s.search(q);


            HandleScope scope;
            //_CLDELETE(q);
            //free(searchString);
            printf("Got %ld hits\n", hits->length());
            // Build the result array
            Local<v8::Array> resultArray = v8::Array::New();
            for (size_t i=0; i < hits->length(); i++) {
                Document& doc(hits->doc(i));
                // {"id":"ab34", "score":1.0}
                Local<Object> resultObject = Object::New();
                // TODO:  This dup might be a leak
                resultObject->Set(String::New("id"), String::New(STRDUP_TtoA(doc.get(_T("_id")))));
                resultObject->Set(String::New("score"), Number::New(hits->score(i)));
                resultArray->Set(i, resultObject);
            }
            baton->results = Persistent<v8::Array>::New(resultArray);

            return 0;
        }

        static int EIO_AfterSearch(eio_req* req)
        {
            HandleScope scope;
            search_baton_t* baton = static_cast<search_baton_t*>(req->data);
            ev_unref(EV_DEFAULT_UC);
            baton->lucene->Unref();

            Handle<Value> argv[2];

            argv[0] = Null(); // Error arg, defaulting to no error
            argv[1] = baton->results;


            TryCatch tryCatch;

            baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

            if (tryCatch.HasCaught()) {
                FatalException(tryCatch);
            }
            
            baton->callback.Dispose();
            baton->results.Dispose();

            delete baton;

            return 0;
        }
};

Persistent<FunctionTemplate> Lucene::s_ct;

extern "C" void init(Handle<Object> target) {
    Lucene::Init(target);
    LuceneDocument::Initialize(target);
}

NODE_MODULE(clucene_bindings, init);
