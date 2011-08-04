class HelloWorld: ObjectWrap {
private:
    int m_count;
public:

    static Handle<Value> NewDocument(const Arguments& args) {
        HandleScope scope;
        Lucene* lucene        = ObjectWrap::Unwrap<Lucene>(args.This());
        lucene->m_count++;
        Local<Document> doc   = _CLNEW Document();
        return scope.Close(doc);
    }

    // args:
    //   Document* doc
    //   String* key
    //   String* value
    //   Integer flags
    static Handle<Value> AddField(const Arguments& args) {
        HandleScope scope;
        Lucene* lucene        = ObjectWrap::Unwrap<Lucene>(args.This());
        lucene->m_count++;

        Local<Document> doc   = args[0];

        TCHAR key[CL_MAX_DIR];
        STRCPY_AtoT(key, *String::Utf8Value(args[1]), CL_MAX_DIR);

        TCHAR value[CL_MAX_DIR];
        STRCPY_AtoT(value, *String::Utf8Value(args[2]), CL_MAX_DIR);

        doc->add(*_CLNEW Field(key, value, args[3]->Int32Value()));
        return scope.Close(Undefined());
    }

    // args:
    //   Document* doc
    //   String* indexPath
    static Handle<Value> AddDocument(const Arguments& args) {
        HandleScope scope;
        IndexWriter* writer   = NULL;
        lucene::analysis::standard::StandardAnalyzer an;

        if (IndexReader::indexExists(*String::Utf8Value(args[1])) ){
            if ( IndexReader::isLocked(*String::Utf8Value(args[1])) ){
                printf("Index was locked... unlocking it.\n");
                IndexReader::unlock(*String::Utf8Value(args[1]));
            }

            writer            = _CLNEW IndexWriter( *String::Utf8Value(args[1]), &an, false);
        } else {
            writer            = _CLNEW IndexWriter( *String::Utf8Value(args[1]) ,&an, true);
        }
    // We can tell the writer to flush at certain occasions
    //writer->setRAMBufferSizeMB(0.5);
    //writer->setMaxBufferedDocs(3);

    // To bypass a possible exception (we have no idea what we will be indexing...)
        writer->setMaxFieldLength(0x7FFFFFFFL); // LUCENE_INT32_MAX_SHOULDBE

    // Turn this off to make indexing faster; we'll turn it on later before optimizing
        writer->setUseCompoundFile(false);

        uint64_t str          = Misc::currentTimeMillis();

        Document* doc         = (args[0];
        writer->addDocument(doc);

    // Make the index use as little files as possible, and optimize it
        writer->setUseCompoundFile(true);
        writer->optimize();

    // Close and clean up
        writer->close();
        _CLLDELETE(writer);

        printf("Indexing took: %d ms.\n\n", (int32_t)(Misc::currentTimeMillis() - str));
        IndexWriter(*String::Utf8Value(args[1]), &an, false);
        Local<int32_t> millis = (int32_t)(Misc::currentTimeMillis() - str);
        return scope.Close(millis);
    }
}
