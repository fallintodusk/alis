BeforeAll {
    . (Join-Path $PSScriptRoot '../canonical_schema_reference_operation.ps1')
}

Describe 'Schema-only World authority link' {
    BeforeEach {
        $ticket=[pscustomobject]@{old_active_sha256=('a'*64);new_active_sha256=('b'*64)}
        $prior=[pscustomobject]@{Sha256=('c'*64);Record=[pscustomobject]@{
            transaction_id=('1'*32);accepted_operation_id=('1'*32);prior_active_set_sha256=('d'*64);
            scopes=@([pscustomobject]@{scope_id='layer_fixture';manifest_sha256=('e'*64)});
            canonical_input_compatibility=@([pscustomobject]@{map_scope_id='map_fixture';
                input_selection_sha256=('f'*64);candidate=[pscustomobject]@{active_sha256=('a'*64);result_sha256=('9'*64)}})
        }}
        $current=[pscustomobject]@{Record=($prior.Record | ConvertTo-Json -Depth 32 | ConvertFrom-Json)}
        $current.Record.transaction_id='2'*32
        $current.Record.accepted_operation_id='2'*32
        $current.Record.prior_active_set_sha256=$prior.Sha256
        $current.Record.canonical_input_compatibility[0].candidate.active_sha256=$ticket.new_active_sha256
    }
    It 'accepts only the exact schema-successor pointer and normal transaction link' {
        {Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $current -Ticket $ticket -OperationId ('2'*32)} | Should -Not -Throw
    }
    It 'rejects a carried manifest change' {
        $current.Record.scopes[0].manifest_sha256='0'*64
        {Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $current -Ticket $ticket -OperationId ('2'*32)} | Should -Throw
    }
    It 'rejects a historical input-selection rewrite' {
        $current.Record.canonical_input_compatibility[0].input_selection_sha256='0'*64
        {Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $current -Ticket $ticket -OperationId ('2'*32)} | Should -Throw
    }
    It 'rejects extra fields or broken transaction ancestry' {
        $current.Record | Add-Member -NotePropertyName ignored_metadata -NotePropertyValue 'forged'
        {Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $current -Ticket $ticket -OperationId ('2'*32)} | Should -Throw
        $current.Record.PSObject.Properties.Remove('ignored_metadata')
        $current.Record.prior_active_set_sha256='0'*64
        {Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $current -Ticket $ticket -OperationId ('2'*32)} | Should -Throw
    }
}
