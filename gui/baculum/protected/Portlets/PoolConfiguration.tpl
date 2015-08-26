<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
	<com:TActivePanel DefaultButton="Apply">
		<strong><%[ Pool name: ]%> <com:TActiveLabel ID="PoolName" /><com:TActiveLabel ID="PoolID" Visible="false" /></strong><br />
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="PoolGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
		<div class="line">
			<div class="text"><com:TLabel ForControl="Enabled" Text="<%[ Enabled: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Enabled" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolumes" Text="<%[ Maximum volumes: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolumes" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="MaxVolumesValidator" ValidationGroup="PoolGroup" ControlToValidate="MaxVolumes" ErrorMessage="<%[ Max volumes value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolumesValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolJobs" Text="<%[ Max vol. jobs: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolJobs" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolJobsValidator" ValidationGroup="PoolGroup" ControlToValidate="MaxVolJobs" ErrorMessage="<%[ Max vol. jobs value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolJobsValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolBytes" Text="<%[ Max vol. bytes: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolBytes" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolBytesValidator" ValidationGroup="PoolGroup" ControlToValidate="MaxVolBytes" ErrorMessage="<%[ Max vol. bytes value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolBytesValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="UseDuration" Text="<%[ Vol. use duration (in hours): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="UseDuration" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="UseDurationValidator" ValidationGroup="PoolGroup" ControlToValidate="UseDuration" ErrorMessage="<%[ Use duration value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="useDurationValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="RetentionPeriod" Text="<%[ Retention period (in hours): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="RetentionPeriod" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="RetentionPeriodValidator" ValidationGroup="PoolGroup" ControlToValidate="RetentionPeriod" ErrorMessage="<%[ Retention period value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="retentionPeriodValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="LabelFormat" Text="<%[ Label format: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="LabelFormat" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="LabelFormatValidator" ValidationGroup="PoolGroup" ControlToValidate="LabelFormat" ErrorMessage="<%[ Label format value must not be empty. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="labelFormatValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="ScratchPool" Text="<%[ Scratch pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="ScratchPool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="RecyclePool" Text="<%[ Recycle pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="RecyclePool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Recycle" Text="<%[ Recycle: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Recycle" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="AutoPrune" Text="<%[ AutoPrune: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="AutoPrune" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="ActionOnPurge" Text="<%[ Action on purge: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="ActionOnPurge" AutoPostBack="false" /></div>
		</div>
		<com:TCallback ID="ReloadPools" OnCallback="Page.PoolWindow.prepareData" ClientSide.OnComplete="SlideWindow.getObj('PoolWindow').setLoadRequest();" />
		<script type="text/javascript">
			var pool_callback_func = function() {
				var mainForm = Prado.Validation.getForm();
				var callback = <%=$this->ReloadPools->ActiveControl->Javascript%>;
				if (Prado.Validation.managers[mainForm].getValidatorsWithError('PoolGroup').length == 0) {
					callback.dispatch();
				}
			}
		</script>
		<div class="button-center">
			<com:BActiveButton ID="RestoreConfiguration" Text="<%[ Restore configuration ]%>" OnClick="restore_configuration" ClientSide.OnSuccess="ConfigurationWindow.getObj('PoolWindow').progress(false); pool_callback_func();" />
			<com:BActiveButton ID="UpdateVolumes" Text="<%[ Update volumes ]%>" OnClick="update_volumes" ClientSide.OnSuccess="ConfigurationWindow.getObj('PoolWindow').progress(false); pool_callback_func();"/>
			<com:BActiveButton ID="Apply" ValidationGroup="PoolGroup" CausesValidation="true" Text="<%[ Apply ]%>" OnClick="apply" ClientSide.OnSuccess="ConfigurationWindow.getObj('PoolWindow').progress(false); pool_callback_func();"/>
		</div>
	</com:TActivePanel>
</com:TContent>
